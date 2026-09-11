using System;
using System.Collections.Generic;
using System.Drawing;
using System.Windows.Forms;

namespace SedaRelayTester
{
    public sealed class MainForm : Form
    {
        private readonly SedaRelayHid relay = new SedaRelayHid();
        private readonly HashSet<Keys> keysDown = new HashSet<Keys>();

        private TextBox logBox;
        private Label statusLabel;
        private Button connectButton;
        private Button allOffButton;

        public MainForm()
        {
            InitializeComponent();
            relay.registerForDeviceNotifications(this.Handle);
            KeyPreview = true;
            KeyDown += MainForm_KeyDown;
            KeyUp += MainForm_KeyUp;
            FormClosing += MainForm_FormClosing;
        }

        protected override void WndProc(ref Message m)
        {
            relay.handleDeviceNotificationMessages(m);
            base.WndProc(ref m);
        }

        private void InitializeComponent()
        {
            Text = "Seda Elektronik USB Role Test";
            StartPosition = FormStartPosition.CenterScreen;
            ClientSize = new Size(760, 520);
            MinimumSize = new Size(760, 520);

            statusLabel = new Label();
            statusLabel.Left = 12;
            statusLabel.Top = 12;
            statusLabel.Width = 720;
            statusLabel.Height = 24;
            statusLabel.Text = "Kart bagli degil";

            connectButton = new Button();
            connectButton.Left = 12;
            connectButton.Top = 44;
            connectButton.Width = 150;
            connectButton.Height = 32;
            connectButton.Text = "Karta Baglan";
            connectButton.Click += delegate { ConnectToDevice(); };

            allOffButton = new Button();
            allOffButton.Left = 172;
            allOffButton.Top = 44;
            allOffButton.Width = 150;
            allOffButton.Height = 32;
            allOffButton.Text = "Tum Roleleri Kapat";
            allOffButton.Click += delegate { SendCommand(0, false, 0xFC, "ALL OFF"); };

            Controls.Add(statusLabel);
            Controls.Add(connectButton);
            Controls.Add(allOffButton);

            AddRelayButton(0, "X / Relay 1", 1, 17, 12, 96);
            AddRelayButton(1, "A / Relay 2", 2, 18, 190, 96);
            AddRelayButton(2, "T / Relay 3", 3, 19, 368, 96);
            AddRelayButton(3, "O / Relay 4", 4, 20, 546, 96);

            Label help = new Label();
            help.Left = 12;
            help.Top = 154;
            help.Width = 720;
            help.Height = 44;
            help.Text = "Klavye: X=Role1, A=Role2, T=Role3, O=Role4. Tus basiliyken ON, birakinca OFF. Mouse ile butona basili tutarak da test edebilirsin.";
            Controls.Add(help);

            logBox = new TextBox();
            logBox.Left = 12;
            logBox.Top = 205;
            logBox.Width = 736;
            logBox.Height = 300;
            logBox.Multiline = true;
            logBox.ScrollBars = ScrollBars.Vertical;
            logBox.ReadOnly = true;
            logBox.Font = new Font("Consolas", 9.0f);
            Controls.Add(logBox);
        }

        private void AddRelayButton(int relayIndex, string text, byte onCommand, byte offCommand, int left, int top)
        {
            Button button = new Button();
            button.Left = left;
            button.Top = top;
            button.Width = 160;
            button.Height = 44;
            button.Text = text;

            button.MouseDown += delegate { SendCommand(relayIndex, true, onCommand, text + " ON"); };
            button.MouseUp += delegate { SendCommand(relayIndex, false, offCommand, text + " OFF"); };
            button.MouseLeave += delegate
            {
                if (MouseButtons == MouseButtons.Left)
                    SendCommand(relayIndex, false, offCommand, text + " OFF");
            };

            Controls.Add(button);
        }

        private void ConnectToDevice()
        {
            relay.Close();

            string message;
            if (relay.Open(out message))
            {
                statusLabel.Text = "Baglandi: " + message;
                Log("CONNECTED: " + message);
            }
            else
            {
                statusLabel.Text = "Baglanti basarisiz";
                Log("CONNECT FAILED: " + message);
            }
        }

        private void SendCommand(int relayIndex, bool on, byte command, string label)
        {
            if (!relay.IsOpen)
            {
                ConnectToDevice();
                if (!relay.IsOpen)
                    return;
            }

            string detail;
            bool ok = relay.SendSedaCommand(command, out detail);
            Log(label + " command=" + command + " result=" + (ok ? "OK" : "FAIL") + " " + detail);
        }

        private void MainForm_KeyDown(object sender, KeyEventArgs e)
        {
            if (keysDown.Contains(e.KeyCode))
                return;

            if (e.KeyCode == Keys.X)
            {
                keysDown.Add(e.KeyCode);
                SendCommand(0, true, 1, "KEY X Relay 1 ON");
                e.Handled = true;
            }
            else if (e.KeyCode == Keys.A)
            {
                keysDown.Add(e.KeyCode);
                SendCommand(1, true, 2, "KEY A Relay 2 ON");
                e.Handled = true;
            }
            else if (e.KeyCode == Keys.T)
            {
                keysDown.Add(e.KeyCode);
                SendCommand(2, true, 3, "KEY T Relay 3 ON");
                e.Handled = true;
            }
            else if (e.KeyCode == Keys.O)
            {
                keysDown.Add(e.KeyCode);
                SendCommand(3, true, 4, "KEY O Relay 4 ON");
                e.Handled = true;
            }
        }

        private void MainForm_KeyUp(object sender, KeyEventArgs e)
        {
            if (!keysDown.Contains(e.KeyCode))
                return;

            keysDown.Remove(e.KeyCode);

            if (e.KeyCode == Keys.X)
            {
                SendCommand(0, false, 17, "KEY X Relay 1 OFF");
                e.Handled = true;
            }
            else if (e.KeyCode == Keys.A)
            {
                SendCommand(1, false, 18, "KEY A Relay 2 OFF");
                e.Handled = true;
            }
            else if (e.KeyCode == Keys.T)
            {
                SendCommand(2, false, 19, "KEY T Relay 3 OFF");
                e.Handled = true;
            }
            else if (e.KeyCode == Keys.O)
            {
                SendCommand(3, false, 20, "KEY O Relay 4 OFF");
                e.Handled = true;
            }
        }

        private void MainForm_FormClosing(object sender, FormClosingEventArgs e)
        {
            relay.Close();
        }

        private void Log(string text)
        {
            logBox.AppendText(DateTime.Now.ToString("HH:mm:ss.fff") + "  " + text + Environment.NewLine);
        }
    }
}
