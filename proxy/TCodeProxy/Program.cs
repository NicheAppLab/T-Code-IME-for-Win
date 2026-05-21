using System;
using System.IO;
using System.IO.Pipes;
using System.Diagnostics;
using System.Threading;
using System.Threading.Tasks;
using System.Net.Http;
using Grpc.Net.Client;
using System.Runtime.InteropServices;
using System.Windows.Forms;
using System.Drawing;

class Program
{
    private const string PipeName = "TCodeEngineProxy";
    private static TCodeService.TCodeServiceClient? _grpcClient;
    private static Process? _javaProcess;
    private static StatusWindow? _statusWindow;
    private static TrayApplicationContext? _trayContext;
    private static int _selectedCandidateIndex = -1;

    public class StatusWindow : Form
    {
        private Label _label;
        public StatusWindow()
        {
            this.FormBorderStyle = FormBorderStyle.None;
            this.TopMost = true;
            this.ShowInTaskbar = false;
            this.BackColor = Color.FromArgb(255, 255, 200); // Pale Yellow
            this.Size = new Size(200, 80); // Taller for candidates
            this.StartPosition = FormStartPosition.Manual;
            this.Location = new Point(100, 100);
            this.Opacity = 0.9; // Slightly transparent

            _label = new Label
            {
                Dock = DockStyle.Fill,
                TextAlign = ContentAlignment.TopLeft,
                Font = new Font("Segoe UI", 11, FontStyle.Bold),
                Padding = new Padding(5),
                Text = ""
            };
            this.Controls.Add(_label);
            
            this.HandleCreated += (s, e) => Console.WriteLine("[Proxy]: Status Window UI Handle Created.");

            // Make it draggable
            _label.MouseDown += (s, e) => {
                if (e.Button == MouseButtons.Left) {
                    ReleaseCapture();
                    SendMessage(Handle, WM_NCLBUTTONDOWN, HT_CAPTION, 0);
                }
            };

            // Force handle creation on the UI thread to prevent cross-thread deadlocks
            // Removed forced handle creation to avoid cross‑thread deadlock
        }

        public void UpdateStatus(string text)
        {
            if (this.IsDisposed) return;
            if (!this.IsHandleCreated) return;

            // Ensure execution on UI thread without blocking the caller
            if (this.InvokeRequired)
            {
                try
                {
                    this.BeginInvoke(new Action(() => UpdateStatus(text)));
                }
                catch (ObjectDisposedException) { /* window disposed */ }
                return;
            }

            try
            {
                if (string.IsNullOrEmpty(text))
                {
                    this.Hide();
                }
                else
                {
                    _label.Text = text;
                    if (!this.Visible) this.Show();
                    this.BringToFront();
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[Proxy-UI]: Update Logic Error: {ex.Message}");
            }
        }

        [DllImport("user32.dll")] public static extern bool ReleaseCapture();
        [DllImport("user32.dll")] public static extern int SendMessage(IntPtr hWnd, int Msg, int wParam, int lParam);
        public const int WM_NCLBUTTONDOWN = 0xA1;
        public const int HT_CAPTION = 0x2;
    }

    public class TrayApplicationContext : ApplicationContext
    {
        private readonly NotifyIcon _notifyIcon;

        public TrayApplicationContext()
        {
            _notifyIcon = new NotifyIcon
            {
                Icon = LoadTrayIcon(),
                Text = "T-Code IME Proxy",
                Visible = true
            };

            var menu = new ContextMenuStrip();
            menu.Items.Add("Show Status", null, (_, _) => ShowStatus());
            menu.Items.Add("Exit", null, (_, _) => ExitThread());
            _notifyIcon.ContextMenuStrip = menu;
            _notifyIcon.DoubleClick += (_, _) => ShowStatus();
            _notifyIcon.ShowBalloonTip(2500, "T-Code IME Proxy", "T-Code IME is running in the tray.", ToolTipIcon.Info);
        }

        private static Icon LoadTrayIcon()
        {
            var iconsDirPath = Path.Combine(AppContext.BaseDirectory, "icons");
            var iconPath = Path.Combine(iconsDirPath, "icon.ico");
            if (!File.Exists(iconPath))
            {
                // Fallback to root output if the icons subfolder wasn't created
                iconPath = Path.Combine(AppContext.BaseDirectory, "icon.ico");
            }

            if (!File.Exists(iconPath))
            {
                return SystemIcons.Application;
            }

            return new Icon(iconPath, 16, 16);
        }

        private void ShowStatus()
        {
            MessageBox.Show("T-Code IME proxy is running in the tray.", "T-Code IME Proxy", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                _notifyIcon.Visible = false;
                _notifyIcon.Dispose();
            }
            base.Dispose(disposing);
        }
    }

    #region Job Object P/Invoke
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    static extern IntPtr CreateJobObject(IntPtr lpJobAttributes, string lpName);

    [DllImport("kernel32.dll")]
    static extern bool SetInformationJobObject(IntPtr hJob, JobObjectInfoType infoType, IntPtr lpJobObjectInfo, uint cbJobObjectInfoLength);

    [DllImport("kernel32.dll")]
    static extern bool AssignProcessToJobObject(IntPtr hJob, IntPtr hProcess);

    public enum JobObjectInfoType { ExtendedLimitInformation = 9 }

    [StructLayout(LayoutKind.Sequential)]
    struct JOBOBJECT_BASIC_LIMIT_INFORMATION
    {
        public Int64 PerProcessUserTimeLimit;
        public Int64 PerJobUserTimeLimit;
        public UInt32 LimitFlags;
        public UIntPtr MinimumWorkingSetSize;
        public UIntPtr MaximumWorkingSetSize;
        public UInt32 ActiveProcessLimit;
        public UInt64 Affinity;
        public UInt32 PriorityClass;
        public UInt32 SchedulingClass;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct IO_COUNTERS
    {
        public UInt64 ReadOperationCount;
        public UInt64 WriteOperationCount;
        public UInt64 OtherOperationCount;
        public UInt64 ReadTransferCount;
        public UInt64 WriteTransferCount;
        public UInt64 OtherTransferCount;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct JOBOBJECT_EXTENDED_LIMIT_INFORMATION
    {
        public JOBOBJECT_BASIC_LIMIT_INFORMATION BasicLimitInformation;
        public IO_COUNTERS IoCounters;
        public UIntPtr ProcessMemoryLimit;
        public UIntPtr JobMemoryLimit;
        public UIntPtr PeakProcessMemoryLimit;
        public UIntPtr PeakJobMemoryLimit;
    }

    const uint JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE = 0x2000;
    private static IntPtr _hJob;
    #endregion

    static async Task Main(string[] args)
    {
        Console.WriteLine("===========================================");
        Console.WriteLine("T-Code IME Proxy v1.2 (Android Logic)");
        Console.WriteLine("Build Date: 2026-05-16");
        Console.WriteLine("===========================================");

        var trayThreadStarted = new ManualResetEventSlim(false);
        var trayThread = new Thread(() =>
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            _trayContext = new TrayApplicationContext();
            trayThreadStarted.Set();
            Application.Run(_trayContext);
        });
        trayThread.SetApartmentState(ApartmentState.STA);
        trayThread.IsBackground = true;
        trayThread.Start();
        trayThreadStarted.Wait();

        // UI thread for Status Window disabled to avoid deadlocks
// var uiThread = new Thread(() => {
//     try {
//         _statusWindow = new StatusWindow();
//         // Run the message loop for the status window
//         Application.Run(_statusWindow);
//     } catch (Exception ex) {
//         Console.WriteLine($"[Proxy-UI]: Error in UI Thread: {ex.Message}");
//     }
// });
// uiThread.SetApartmentState(ApartmentState.STA);
// uiThread.IsBackground = true; // Do not block process exit
// uiThread.Start();

        // Setup Job Object for automatic cleanup
        _hJob = CreateJobObject(IntPtr.Zero, "TCodeEngineJob");
        var info = new JOBOBJECT_EXTENDED_LIMIT_INFORMATION();
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        int length = Marshal.SizeOf(typeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION));
        IntPtr ptr = Marshal.AllocHGlobal(length);
        try {
            Marshal.StructureToPtr(info, ptr, false);
            if (!SetInformationJobObject(_hJob, JobObjectInfoType.ExtendedLimitInformation, ptr, (uint)length))
                Console.WriteLine("[Proxy Warning]: Failed to set job object information.");
        } finally { Marshal.FreeHGlobal(ptr); }

        // Force HTTP/2 without TLS (Prior Knowledge)
        AppContext.SetSwitch("System.Net.Http.SocketsHttpHandler.Http2UnencryptedSupport", true);

        // Determine gRPC server port (env `TCODE_SERVER_PORT` overrides default)
        var grpcPortEnv = Environment.GetEnvironmentVariable("TCODE_SERVER_PORT");
        var grpcPort = 57001;
        if (!string.IsNullOrEmpty(grpcPortEnv) && int.TryParse(grpcPortEnv, out var parsedPort)) grpcPort = parsedPort;
        var grpcAddress = $"http://localhost:{grpcPort}";
        Console.WriteLine($"Using gRPC address: {grpcAddress}");

        // 1. Setup gRPC with H2C support (unencrypted HTTP/2)
        var channel = GrpcChannel.ForAddress(grpcAddress, new GrpcChannelOptions
        {
            HttpHandler = new SocketsHttpHandler
            {
                EnableMultipleHttp2Connections = true,
                DefaultProxyCredentials = null,
                UseProxy = false
            }
        });
        _grpcClient = new TCodeService.TCodeServiceClient(channel);

        // 2. Start Java Engine
        await EnsureEngineRunningAsync();

        // 3. Start Multi-threaded Pipe Server Loop
        Console.WriteLine($"Listening on Named Pipe: \\\\.\\pipe\\{PipeName}");
        _ = Task.Run(async () =>
        {
            while (true)
            {
                try
                {
                    var pipeServer = new NamedPipeServerStream(PipeName, PipeDirection.InOut, 
                        NamedPipeServerStream.MaxAllowedServerInstances, PipeTransmissionMode.Byte, PipeOptions.Asynchronous);
                    
                    await pipeServer.WaitForConnectionAsync();
                    
                    // Handle each client in its own task
                    _ = Task.Run(async () => 
                    {
                        Console.WriteLine($"[Proxy]: Client Connected!");
                        await HandleClientAsync(pipeServer);
                        Console.WriteLine($"[Proxy]: Client Disconnected.");
                    });
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Pipe Listener Error: {ex.Message}");
                    await Task.Delay(1000);
                }
            }
        });

        // Keep Main alive
        await Task.Delay(-1);
    }

    private static async Task HandleClientAsync(NamedPipeServerStream pipe)
    {
        using var reader = new BinaryReader(pipe);
        using var writer = new BinaryWriter(pipe);

        try
        {
            string respBuffer = "";
            List<string> respCandidates = new List<string>();

            while (pipe.IsConnected)
            {
                try
                {
                    // Read Request (8 bytes)
                    Console.WriteLine("[Proxy]: Waiting for next command from pipe...");
                    uint type = reader.ReadUInt32();
                    uint vkey = reader.ReadUInt32();

                    Console.WriteLine($"[Proxy]: Command {type} received (Data: {vkey}). Processing...");

                    bool success = false;
                    BufferStatusResponse? resp = null;

                    string committed = "";
                    string composition = "";

                    // Advanced Control Logic (Parity with Android TCodeProcessor.kt)
                    Console.WriteLine($"[Proxy]: Executing command for vkey {vkey}...");
                    // Increase deadline while debugging to avoid transient timeouts
                    var timeout = DateTime.UtcNow.AddSeconds(5);
                    
                    try {
                        if (vkey == 8) // VK_BACK
                        {
                            resp = await _grpcClient!.BackspaceAsync(new BackspaceRequest(), deadline: timeout);
                            success = resp?.CommandSucceed ?? false;
                        }
                        else if (vkey == 32) // VK_SPACE
                        {
                            if (respCandidates.Count > 0)
                            {
                                _selectedCandidateIndex = (_selectedCandidateIndex + 1) % respCandidates.Count;
                                success = true;
                            }
                            else if (string.IsNullOrEmpty(respBuffer))
                            {
                                committed = " ";
                                success = true;
                            }
                            else if (!respBuffer.Contains('△'))
                            {
                                resp = await _grpcClient!.ConvertAsync(new ConvertRequest(), deadline: timeout);
                                var commitResp = await _grpcClient!.CommitAsync(new CommitRequest(), deadline: timeout);
                                committed = commitResp.Output;
                                success = resp?.CommandSucceed ?? true;
                            }
                            else if (respBuffer.Contains('▲'))
                            {
                                // mazegaki and kanji composition mode
                                // consume kanji composition only
                                resp = await _grpcClient!.ConvertAsync(new ConvertRequest(), deadline: timeout);
                                success = resp?.CommandSucceed ?? true;
                            }
                            else
                            {
                                // mazegaki and not kanji composition mode
                                resp = await _grpcClient!.ConvertAsync(new ConvertRequest(), deadline: timeout);
                                if (resp.Candidates.Count > 0)
                                {
                                    _selectedCandidateIndex = (resp.Candidates.Count == 1) ? 0 : -1;
                                }
                                success = resp?.CommandSucceed ?? true;
                            }
                        }
                        else if (vkey == 37 || vkey == 39) // VK_LEFT, VK_RIGHT
                        {
                            if (respCandidates.Count > 0)
                            {
                                if (vkey == 39) _selectedCandidateIndex = (_selectedCandidateIndex + 1) % respCandidates.Count;
                                else _selectedCandidateIndex = (_selectedCandidateIndex <= 0) ? respCandidates.Count - 1 : _selectedCandidateIndex - 1;
                                success = true;
                            }
                            else if (!string.IsNullOrEmpty(respBuffer))
                            {
                                if (vkey == 39) resp = await _grpcClient!.RightAsync(new InflexRightRequest(), deadline: timeout);
                                else resp = await _grpcClient!.LeftAsync(new InflexLeftRequest(), deadline: timeout);
                                success = resp?.CommandSucceed ?? true;
                            }
                        }
                        else if (vkey == 13) // VK_RETURN
                        {
                            if (respCandidates.Count > 0)
                            {
                                int idx = _selectedCandidateIndex >= 0 ? _selectedCandidateIndex : 0;
                                resp = await _grpcClient!.SelectAsync(new SelectCandidateRequest { N = idx }, deadline: timeout);
                                var commitResp = await _grpcClient!.CommitAsync(new CommitRequest(), deadline: timeout);
                                committed = commitResp.Output;
                                _selectedCandidateIndex = -1;
                                success = resp?.CommandSucceed ?? true;
                            }
                            else if (!string.IsNullOrEmpty(respBuffer))
                            {
                                var commitResp = await _grpcClient!.CommitAsync(new CommitRequest(), deadline: timeout);
                                committed = commitResp.Output;
                                success = true;
                            }
                        }
                        else if ((vkey >= 48 && vkey <= 57) || (vkey >= 65 && vkey <= 90) || (vkey >= 186 && vkey <= 222))
                        {
                            char c = '\0';
                            if (vkey >= 65 && vkey <= 90) c = (char)(vkey + 32); // a-z
                            else if (vkey >= 48 && vkey <= 57) c = (char)vkey; // 0-9
                            else {
                                // Punctuation mapping (standard US layout)
                                c = vkey switch {
                                    186 => ';', 187 => '=', 188 => ',', 189 => '-', 190 => '.', 191 => '/', 
                                    192 => '`', 219 => '[', 220 => '\\', 221 => ']', 222 => '\'',
                                    _ => '\0'
                                };
                            }

                            if (c != '\0') {
                                resp = await _grpcClient!.PutAsync(new PutRequest { Char = c.ToString() }, deadline: timeout);
                                committed = resp.OutputBuffer;
                                _selectedCandidateIndex = -1;
                                success = resp?.CommandSucceed ?? true;
                            }
                        }
                        else if (type == 2) // Reset
                        {
                            await _grpcClient!.ResetAsync(new ResetRequest(), deadline: timeout);
                            success = true;
                        }
                    } catch (Exception ex) {
                        Console.WriteLine($"[Proxy-Engine]: Engine Call Failed: {ex}");
                    }

                    bool gotResponse = resp != null;
                    if (gotResponse)
                    {
                        respBuffer = resp.Buffer ?? "";
                        respCandidates = resp.Candidates?.ToList() ?? new List<string>();
                    }

                    string lastKeyChar = resp?.LastCharAsKey ?? "";
                    if (lastKeyChar.StartsWith("Some(")) lastKeyChar = lastKeyChar.Substring(5, 1);
                    else if (lastKeyChar == "None") lastKeyChar = "";

                    composition = respBuffer + (string.IsNullOrEmpty(lastKeyChar) ? "" : $"({lastKeyChar})");

                    if (respCandidates.Count > 0)
                    {
                        composition += " [" + string.Join(" ", respCandidates.Select((c, i) => i == _selectedCandidateIndex ? $"[{i + 1}:{c}]" : $"{i + 1}:{c}")) + "]";
                    }

                    if (success)
                    {
                        Console.WriteLine("[Proxy]: Engine call successful. Updating state...");
                        // Diagnostic: print the raw engine response for troubleshooting conversion
                        Console.WriteLine($"[Proxy-EngineResp]: Buffer='{resp?.Buffer ?? ""}', Candidates={resp?.Candidates?.Count ?? 0}, CommandSucceed={resp?.CommandSucceed}");

                        // If we just committed text, reset the engine for the next sequence
                        if (!string.IsNullOrEmpty(committed))
                        {
                            composition = ""; // Force empty on commit to prevent duplicates
                            Console.WriteLine("[Proxy]: Resetting engine after commit...");
                            await _grpcClient!.ResetAsync(new ResetRequest(), deadline: timeout);
                            respBuffer = "";
                            respCandidates.Clear();
                        }
                    }
                    else
                    {
                        Console.WriteLine("[Proxy]: Engine call did NOT succeed. Logging response details...");
                        if (resp == null)
                        {
                            Console.WriteLine("[Proxy-EngineResp-Fail]: resp==null");
                        }
                        else
                        {
                            Console.WriteLine($"[Proxy-EngineResp-Fail]: Buffer='{resp.Buffer}', Candidates={resp.Candidates?.Count ?? 0}, CommandSucceed={resp.CommandSucceed}");
                        }
                    }

                    // Update Status Window with Candidates whenever we have fresh engine state
                    string statusText = composition;
                    if (respCandidates.Count > 0)
                    {
                        statusText += "\n" + string.Join(" ", respCandidates.Select((c, i) => i == _selectedCandidateIndex ? $"[{c}]" : c));
                    }
                    _statusWindow?.UpdateStatus(statusText);

                    Console.WriteLine("[Proxy]: Sending response back to pipe...");
                    // Prepare the 1032-byte response buffer
                    byte[] responseBuffer = new byte[1032];
                    Array.Clear(responseBuffer, 0, responseBuffer.Length);

                    uint successVal = success ? 1u : 0u;
                    BitConverter.TryWriteBytes(responseBuffer.AsSpan(0, 4), successVal);

                    uint activeVal = (respBuffer.Length > 0 || respCandidates.Count > 0 || !string.IsNullOrEmpty(committed)) ? 1u : 0u;
                    BitConverter.TryWriteBytes(responseBuffer.AsSpan(4, 4), activeVal);
                    
                    byte[] committedBytes = System.Text.Encoding.Unicode.GetBytes(committed);
                    int committedCount = Math.Min(committedBytes.Length, 510);
                    Buffer.BlockCopy(committedBytes, 0, responseBuffer, 8, committedCount);

                    byte[] compositionBytes = System.Text.Encoding.Unicode.GetBytes(composition);
                    int compositionCount = Math.Min(compositionBytes.Length, 510);
                    Buffer.BlockCopy(compositionBytes, 0, responseBuffer, 520, compositionCount);

                    pipe.Write(responseBuffer, 0, responseBuffer.Length);
                    pipe.Flush(); 
                    Console.WriteLine("[Proxy]: Response sent to IME.");
                }
                catch (EndOfStreamException)
                {
                    break;
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"[Proxy ERROR]: {ex.Message}");
                    break;
                }
            }
        }
        catch (EndOfStreamException) { /* Client disconnected */ }
    }

    private static async Task EnsureEngineRunningAsync()
    {
        if (_javaProcess != null && !_javaProcess.HasExited) return;


        // Robustly find the 'engine' directory by searching upwards
        string? currentDir = AppDomain.CurrentDomain.BaseDirectory;
        string? engineDir = null;
        while (currentDir != null)
        {
            var potential = Path.Combine(currentDir, "engine");
            if (Directory.Exists(potential))
            {
                engineDir = potential;
                break;
            }
            currentDir = Path.GetDirectoryName(currentDir);
        }

        if (engineDir == null)
        {
            Console.WriteLine("Error: Could not find 'engine' directory in any parent folder.");
            return;
        }

        string scriptPath = Path.Combine(engineDir, "bin", "tcodeserver.bat");
        if (!File.Exists(scriptPath))
        {
            Console.WriteLine($"Error: Could not find engine script at {scriptPath}");
            return;
        }

        Console.WriteLine($"Starting Engine via Script: {scriptPath}");

        // Allow overriding port and dict-dir via environment. Defaults provided by the server update.
        var serverPortEnv = Environment.GetEnvironmentVariable("TCODE_SERVER_PORT") ?? "57001";
        var serverPort = serverPortEnv;
        var dictDirEnv = Environment.GetEnvironmentVariable("TCODE_DICT_DIR") ?? "%APPDATA%\\tcode-server\\dictionary";
        var dictDirResolved = Environment.ExpandEnvironmentVariables(dictDirEnv);

        Console.WriteLine($"Engine config: port={serverPort}, dict-dir={dictDirResolved}");

        _javaProcess = new Process
        {
            StartInfo = new ProcessStartInfo
            {
                FileName = "cmd.exe",
                // Pass system properties to the JVM via JAVA_OPTS so the server picks them up
                Arguments = $"/c \"set JAVA_OPTS=-Duser.home=\"{engineDir}\" -Dtcode-server.server.port={serverPort} -Dtcode-server.dict-dir=\"{dictDirResolved}\" && \"{scriptPath}\"\"",
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                CreateNoWindow = true,
                WorkingDirectory = engineDir
            }
        };

        _javaProcess.OutputDataReceived += (s, e) => Console.WriteLine($"[Engine]: {e.Data}");
        _javaProcess.ErrorDataReceived += (s, e) => Console.WriteLine($"[Engine ERROR]: {e.Data}");

        _javaProcess.Start();
        try { AssignProcessToJobObject(_hJob, _javaProcess.Handle); } catch { /* Ignore if fails */ }
        _javaProcess.BeginOutputReadLine();
        _javaProcess.BeginErrorReadLine();
        // Give it a moment to boot
        await Task.Delay(2000);
    }
}
