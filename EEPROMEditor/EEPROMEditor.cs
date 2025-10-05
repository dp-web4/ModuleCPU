using System;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace EEPROMEditor
{
    public partial class MainForm : Form
    {
        // EEPROM size and offsets from EEPROM.h
        private const int EEPROM_SIZE = 2048;
        private const int EEPROM_UNIQUE_ID = 0x0000;
        private const int EEPROM_EXPECTED_CELL_COUNT = 0x0004;
        private const int EEPROM_MAX_CHARGE_CURRENT = 0x0005;
        private const int EEPROM_MAX_DISCHARGE_CURRENT = 0x0007;
        private const int EEPROM_SEQUENTIAL_COUNT_MISMATCH = 0x0009;
        private const int EEPROM_METADATA_SIZE = 64;

        // Frame counter area
        private const int EEPROM_FRAME_COUNTER_BASE = 0x0040;
        private const int EEPROM_FRAME_COUNTER_SIZE = 512;
        private const int COUNTER_POSITIONS = 128;
        private const int BYTES_PER_COUNTER = 4;

        private byte[] eepromData = new byte[EEPROM_SIZE];
        private string currentFileName = "";

        // Controls
        private TextBox txtUniqueId;
        private NumericUpDown numCellCount;
        private NumericUpDown numMaxCharge;
        private NumericUpDown numMaxDischarge;
        private NumericUpDown numCountMismatch;
        private DataGridView dgvFrameCounters;
        private TextBox txtCurrentCounter;
        private TextBox txtCurrentPosition;
        private Label lblFileName;
        private RichTextBox txtHexView;
        private TabControl tabControl;

        public MainForm()
        {
            InitializeComponent();
            InitializeEEPROM();
        }

        private void InitializeComponent()
        {
            this.Text = "EEPROM Editor - ModuleCPU";
            this.Size = new Size(900, 700);
            this.StartPosition = FormStartPosition.CenterScreen;

            // Menu
            var menuStrip = new MenuStrip();
            var fileMenu = new ToolStripMenuItem("File");

            var openMenuItem = new ToolStripMenuItem("Open .eep file...", null, OpenFile_Click);
            openMenuItem.ShortcutKeys = Keys.Control | Keys.O;

            var saveMenuItem = new ToolStripMenuItem("Save .eep file...", null, SaveFile_Click);
            saveMenuItem.ShortcutKeys = Keys.Control | Keys.S;

            var newMenuItem = new ToolStripMenuItem("New EEPROM", null, NewFile_Click);
            newMenuItem.ShortcutKeys = Keys.Control | Keys.N;

            fileMenu.DropDownItems.Add(newMenuItem);
            fileMenu.DropDownItems.Add(new ToolStripSeparator());
            fileMenu.DropDownItems.Add(openMenuItem);
            fileMenu.DropDownItems.Add(saveMenuItem);
            fileMenu.DropDownItems.Add(new ToolStripSeparator());
            fileMenu.DropDownItems.Add(new ToolStripMenuItem("Exit", null, (s, e) => Close()));

            menuStrip.Items.Add(fileMenu);
            this.MainMenuStrip = menuStrip;
            this.Controls.Add(menuStrip);

            // Status label
            lblFileName = new Label
            {
                Text = "No file loaded",
                Location = new Point(10, 35),
                Size = new Size(860, 20),
                BorderStyle = BorderStyle.Fixed3D
            };
            this.Controls.Add(lblFileName);

            // Tab control
            tabControl = new TabControl
            {
                Location = new Point(10, 60),
                Size = new Size(860, 580)
            };

            // Metadata tab
            var metadataTab = new TabPage("Metadata Fields");
            CreateMetadataControls(metadataTab);
            tabControl.TabPages.Add(metadataTab);

            // Frame counter tab
            var counterTab = new TabPage("Frame Counter Buffer");
            CreateFrameCounterControls(counterTab);
            tabControl.TabPages.Add(counterTab);

            // Hex view tab
            var hexTab = new TabPage("Hex View");
            CreateHexViewControls(hexTab);
            tabControl.TabPages.Add(hexTab);

            this.Controls.Add(tabControl);

            // Buttons at bottom
            var btnApply = new Button
            {
                Text = "Apply Changes",
                Location = new Point(700, 645),
                Size = new Size(100, 30)
            };
            btnApply.Click += ApplyChanges_Click;
            this.Controls.Add(btnApply);

            var btnRefresh = new Button
            {
                Text = "Refresh View",
                Location = new Point(590, 645),
                Size = new Size(100, 30)
            };
            btnRefresh.Click += RefreshView_Click;
            this.Controls.Add(btnRefresh);
        }

        private void CreateMetadataControls(TabPage tab)
        {
            int y = 20;

            // Unique ID
            var lblId = new Label
            {
                Text = "Unique ID (hex):",
                Location = new Point(20, y),
                Size = new Size(150, 20)
            };
            tab.Controls.Add(lblId);

            txtUniqueId = new TextBox
            {
                Location = new Point(180, y),
                Size = new Size(100, 20)
            };
            tab.Controls.Add(txtUniqueId);

            y += 30;

            // Expected cell count
            var lblCells = new Label
            {
                Text = "Expected Cell Count:",
                Location = new Point(20, y),
                Size = new Size(150, 20)
            };
            tab.Controls.Add(lblCells);

            numCellCount = new NumericUpDown
            {
                Location = new Point(180, y),
                Size = new Size(100, 20),
                Minimum = 0,
                Maximum = 255
            };
            tab.Controls.Add(numCellCount);

            y += 30;

            // Max charge current
            var lblCharge = new Label
            {
                Text = "Max Charge Current (A):",
                Location = new Point(20, y),
                Size = new Size(150, 20)
            };
            tab.Controls.Add(lblCharge);

            numMaxCharge = new NumericUpDown
            {
                Location = new Point(180, y),
                Size = new Size(100, 20),
                Minimum = 0,
                Maximum = 65535,
                DecimalPlaces = 1
            };
            tab.Controls.Add(numMaxCharge);

            y += 30;

            // Max discharge current
            var lblDischarge = new Label
            {
                Text = "Max Discharge Current (A):",
                Location = new Point(20, y),
                Size = new Size(150, 20)
            };
            tab.Controls.Add(lblDischarge);

            numMaxDischarge = new NumericUpDown
            {
                Location = new Point(180, y),
                Size = new Size(100, 20),
                Minimum = 0,
                Maximum = 65535,
                DecimalPlaces = 1
            };
            tab.Controls.Add(numMaxDischarge);

            y += 30;

            // Sequential count mismatch
            var lblMismatch = new Label
            {
                Text = "Count Mismatch:",
                Location = new Point(20, y),
                Size = new Size(150, 20)
            };
            tab.Controls.Add(lblMismatch);

            numCountMismatch = new NumericUpDown
            {
                Location = new Point(180, y),
                Size = new Size(100, 20),
                Minimum = 0,
                Maximum = 65535
            };
            tab.Controls.Add(numCountMismatch);

            y += 50;

            // Info label
            var lblInfo = new Label
            {
                Text = $"Metadata uses bytes 0x0000-0x003F ({EEPROM_METADATA_SIZE} bytes)\n" +
                       $"Current usage: 11 bytes\n" +
                       $"Available: {EEPROM_METADATA_SIZE - 11} bytes",
                Location = new Point(20, y),
                Size = new Size(400, 60),
                BorderStyle = BorderStyle.FixedSingle
            };
            tab.Controls.Add(lblInfo);
        }

        private void CreateFrameCounterControls(TabPage tab)
        {
            // Current counter info
            var lblCurrent = new Label
            {
                Text = "Current Frame Counter:",
                Location = new Point(20, 20),
                Size = new Size(150, 20)
            };
            tab.Controls.Add(lblCurrent);

            txtCurrentCounter = new TextBox
            {
                Location = new Point(180, 20),
                Size = new Size(100, 20),
                ReadOnly = true
            };
            tab.Controls.Add(txtCurrentCounter);

            var lblPosition = new Label
            {
                Text = "Current Position:",
                Location = new Point(320, 20),
                Size = new Size(100, 20)
            };
            tab.Controls.Add(lblPosition);

            txtCurrentPosition = new TextBox
            {
                Location = new Point(430, 20),
                Size = new Size(60, 20),
                ReadOnly = true
            };
            tab.Controls.Add(txtCurrentPosition);

            // Set counter value controls
            var lblSetCounter = new Label
            {
                Text = "Set Counter Value:",
                Location = new Point(520, 20),
                Size = new Size(120, 20)
            };
            tab.Controls.Add(lblSetCounter);

            var numSetCounter = new NumericUpDown
            {
                Location = new Point(650, 20),
                Size = new Size(100, 20),
                Minimum = 0,
                Maximum = uint.MaxValue,
                Value = 0
            };
            tab.Controls.Add(numSetCounter);

            var btnSetCounter = new Button
            {
                Text = "Set",
                Location = new Point(760, 18),
                Size = new Size(50, 24)
            };
            btnSetCounter.Click += (s, e) => SetCounterValue((uint)numSetCounter.Value);
            tab.Controls.Add(btnSetCounter);

            // DataGridView for all counter positions
            dgvFrameCounters = new DataGridView
            {
                Location = new Point(20, 60),
                Size = new Size(800, 450),
                AllowUserToAddRows = false,
                AllowUserToDeleteRows = false,
                ReadOnly = true,
                ColumnHeadersHeightSizeMode = DataGridViewColumnHeadersHeightSizeMode.AutoSize
            };

            dgvFrameCounters.Columns.Add("Position", "Position");
            dgvFrameCounters.Columns.Add("Address", "Address (hex)");
            dgvFrameCounters.Columns.Add("Value", "Counter Value");
            dgvFrameCounters.Columns.Add("Hex", "Hex");
            dgvFrameCounters.Columns.Add("Status", "Status");

            dgvFrameCounters.Columns["Position"].Width = 80;
            dgvFrameCounters.Columns["Address"].Width = 100;
            dgvFrameCounters.Columns["Value"].Width = 150;
            dgvFrameCounters.Columns["Hex"].Width = 100;
            dgvFrameCounters.Columns["Status"].Width = 300;

            tab.Controls.Add(dgvFrameCounters);

            // Info label
            var lblInfo = new Label
            {
                Text = "Frame Counter Buffer uses bytes 0x0040-0x023F (512 bytes)\n" +
                       "128 positions × 4 bytes each\n" +
                       "Wear leveling: Rotates every 256 increments\n" +
                       "Endurance: ~25.9 years continuous operation at 2Hz",
                Location = new Point(20, 520),
                Size = new Size(800, 60),
                BorderStyle = BorderStyle.FixedSingle
            };
            tab.Controls.Add(lblInfo);
        }

        private void CreateHexViewControls(TabPage tab)
        {
            txtHexView = new RichTextBox
            {
                Location = new Point(10, 10),
                Size = new Size(830, 550),
                Font = new Font("Consolas", 9),
                ReadOnly = true,
                WordWrap = false
            };
            tab.Controls.Add(txtHexView);
        }

        private void InitializeEEPROM()
        {
            // Initialize with 0xFF (unprogrammed EEPROM state)
            for (int i = 0; i < EEPROM_SIZE; i++)
            {
                eepromData[i] = 0xFF;
            }
            RefreshView();
        }

        private void OpenFile_Click(object sender, EventArgs e)
        {
            using (var dialog = new OpenFileDialog())
            {
                dialog.Filter = "EEPROM files (*.eep)|*.eep|Hex files (*.hex)|*.hex|All files (*.*)|*.*";
                dialog.Title = "Open EEPROM File";

                if (dialog.ShowDialog() == DialogResult.OK)
                {
                    LoadEEPROMFile(dialog.FileName);
                }
            }
        }

        private void SaveFile_Click(object sender, EventArgs e)
        {
            using (var dialog = new SaveFileDialog())
            {
                dialog.Filter = "EEPROM files (*.eep)|*.eep|Hex files (*.hex)|*.hex";
                dialog.Title = "Save EEPROM File";
                dialog.DefaultExt = "eep";

                if (!string.IsNullOrEmpty(currentFileName))
                {
                    dialog.FileName = Path.GetFileName(currentFileName);
                }

                if (dialog.ShowDialog() == DialogResult.OK)
                {
                    SaveEEPROMFile(dialog.FileName);
                }
            }
        }

        private void NewFile_Click(object sender, EventArgs e)
        {
            if (MessageBox.Show("Create new EEPROM? This will clear all current data.",
                "New EEPROM", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes)
            {
                InitializeEEPROM();
                currentFileName = "";
                lblFileName.Text = "New EEPROM (not saved)";
            }
        }

        private void LoadEEPROMFile(string filename)
        {
            try
            {
                var lines = File.ReadAllLines(filename);
                Array.Clear(eepromData, 0, EEPROM_SIZE);

                foreach (var line in lines)
                {
                    if (line.StartsWith(":"))
                    {
                        var data = line.Substring(1);
                        int byteCount = Convert.ToInt32(data.Substring(0, 2), 16);
                        int address = Convert.ToInt32(data.Substring(2, 4), 16);
                        int recordType = Convert.ToInt32(data.Substring(6, 2), 16);

                        if (recordType == 0) // Data record
                        {
                            for (int i = 0; i < byteCount; i++)
                            {
                                int byteValue = Convert.ToInt32(data.Substring(8 + i * 2, 2), 16);
                                if (address + i < EEPROM_SIZE)
                                {
                                    eepromData[address + i] = (byte)byteValue;
                                }
                            }
                        }
                    }
                }

                currentFileName = filename;
                lblFileName.Text = $"File: {Path.GetFileName(filename)}";
                RefreshView();
                MessageBox.Show($"Loaded {filename}", "Success", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error loading file: {ex.Message}", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void SaveEEPROMFile(string filename)
        {
            try
            {
                using (var writer = new StreamWriter(filename))
                {
                    int address = 0;
                    while (address < EEPROM_SIZE)
                    {
                        int chunkSize = Math.Min(16, EEPROM_SIZE - address);
                        var line = new StringBuilder(":");

                        line.AppendFormat("{0:X2}", chunkSize);
                        line.AppendFormat("{0:X4}", address);
                        line.Append("00"); // Record type (data)

                        int checksum = chunkSize + (address >> 8) + (address & 0xFF);

                        for (int i = 0; i < chunkSize; i++)
                        {
                            byte b = eepromData[address + i];
                            line.AppendFormat("{0:X2}", b);
                            checksum += b;
                        }

                        checksum = (256 - (checksum & 0xFF)) & 0xFF;
                        line.AppendFormat("{0:X2}", checksum);

                        writer.WriteLine(line.ToString());
                        address += chunkSize;
                    }

                    // End of file record
                    writer.WriteLine(":00000001FF");
                }

                currentFileName = filename;
                lblFileName.Text = $"File: {Path.GetFileName(filename)} (saved)";
                MessageBox.Show($"Saved to {filename}", "Success", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error saving file: {ex.Message}", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void RefreshView()
        {
            RefreshMetadataFields();
            RefreshFrameCounterView();
            RefreshHexView();
        }

        private void RefreshMetadataFields()
        {
            // Read unique ID (4 bytes)
            uint uniqueId = BitConverter.ToUInt32(eepromData, EEPROM_UNIQUE_ID);
            txtUniqueId.Text = uniqueId.ToString("X8");

            // Read cell count (1 byte)
            numCellCount.Value = eepromData[EEPROM_EXPECTED_CELL_COUNT];

            // Read max charge current (2 bytes)
            ushort maxCharge = BitConverter.ToUInt16(eepromData, EEPROM_MAX_CHARGE_CURRENT);
            numMaxCharge.Value = maxCharge / 10m; // Convert to amps

            // Read max discharge current (2 bytes)
            ushort maxDischarge = BitConverter.ToUInt16(eepromData, EEPROM_MAX_DISCHARGE_CURRENT);
            numMaxDischarge.Value = maxDischarge / 10m; // Convert to amps

            // Read count mismatch (2 bytes)
            ushort countMismatch = BitConverter.ToUInt16(eepromData, EEPROM_SEQUENTIAL_COUNT_MISMATCH);
            numCountMismatch.Value = countMismatch;
        }

        private void RefreshFrameCounterView()
        {
            dgvFrameCounters.Rows.Clear();

            uint maxCounter = 0;
            int maxPosition = -1;
            bool foundValid = false;

            for (int pos = 0; pos < COUNTER_POSITIONS; pos++)
            {
                int addr = EEPROM_FRAME_COUNTER_BASE + (pos * BYTES_PER_COUNTER);

                // Read counter value (MSB first)
                uint value = (uint)eepromData[addr] << 24;
                value |= (uint)eepromData[addr + 1] << 16;
                value |= (uint)eepromData[addr + 2] << 8;
                value |= (uint)eepromData[addr + 3];

                string status = "";
                if (value == 0xFFFFFFFF)
                {
                    status = "Unprogrammed/Invalid";
                }
                else if (!foundValid || value > maxCounter)
                {
                    maxCounter = value;
                    maxPosition = pos;
                    foundValid = true;
                    status = "← Current (highest value)";
                }

                dgvFrameCounters.Rows.Add(
                    pos.ToString(),
                    $"0x{addr:X4}",
                    value == 0xFFFFFFFF ? "INVALID" : value.ToString(),
                    $"{value:X8}",
                    status
                );
            }

            // Highlight current position
            if (maxPosition >= 0)
            {
                dgvFrameCounters.Rows[maxPosition].DefaultCellStyle.BackColor = Color.LightGreen;
                txtCurrentCounter.Text = maxCounter.ToString();
                txtCurrentPosition.Text = maxPosition.ToString();
            }
            else
            {
                txtCurrentCounter.Text = "None found";
                txtCurrentPosition.Text = "N/A";
            }
        }

        private void SetCounterValue(uint newValue)
        {
            // Find current position (same logic as RefreshFrameCounterView)
            uint maxCounter = 0;
            int maxPosition = -1;
            bool foundValid = false;

            for (int pos = 0; pos < COUNTER_POSITIONS; pos++)
            {
                int addr = EEPROM_FRAME_COUNTER_BASE + (pos * BYTES_PER_COUNTER);
                uint value = (uint)eepromData[addr] << 24;
                value |= (uint)eepromData[addr + 1] << 16;
                value |= (uint)eepromData[addr + 2] << 8;
                value |= (uint)eepromData[addr + 3];

                if (value != 0xFFFFFFFF && (!foundValid || value > maxCounter))
                {
                    maxCounter = value;
                    maxPosition = pos;
                    foundValid = true;
                }
            }

            // If no valid position found, use position 0
            if (maxPosition < 0)
            {
                maxPosition = 0;
            }

            // Write new value at current position (MSB first)
            int writeAddr = EEPROM_FRAME_COUNTER_BASE + (maxPosition * BYTES_PER_COUNTER);
            eepromData[writeAddr] = (byte)(newValue >> 24);
            eepromData[writeAddr + 1] = (byte)(newValue >> 16);
            eepromData[writeAddr + 2] = (byte)(newValue >> 8);
            eepromData[writeAddr + 3] = (byte)(newValue);

            // Refresh the display
            RefreshView_Click(null, null);

            MessageBox.Show($"Set frame counter to {newValue} at position {maxPosition}",
                          "Counter Updated", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }

        private void RefreshHexView()
        {
            txtHexView.Clear();
            var sb = new StringBuilder();

            for (int i = 0; i < EEPROM_SIZE; i += 16)
            {
                // Address
                sb.AppendFormat("{0:X4}: ", i);

                // Hex bytes
                for (int j = 0; j < 16 && i + j < EEPROM_SIZE; j++)
                {
                    sb.AppendFormat("{0:X2} ", eepromData[i + j]);
                }

                // Spacing
                sb.Append("  ");

                // ASCII representation
                for (int j = 0; j < 16 && i + j < EEPROM_SIZE; j++)
                {
                    byte b = eepromData[i + j];
                    sb.Append(b >= 32 && b < 127 ? (char)b : '.');
                }

                sb.AppendLine();
            }

            txtHexView.Text = sb.ToString();

            // Highlight different sections with colors
            HighlightSection(0, EEPROM_METADATA_SIZE * 3, Color.LightBlue); // Metadata
            HighlightSection(EEPROM_FRAME_COUNTER_BASE * 3, EEPROM_FRAME_COUNTER_SIZE * 3, Color.LightGreen); // Frame counter
        }

        private void HighlightSection(int start, int length, Color color)
        {
            // Note: This is simplified - actual implementation would need proper offset calculation
            // considering the hex view format
        }

        private void ApplyChanges_Click(object sender, EventArgs e)
        {
            try
            {
                // Write unique ID
                uint uniqueId = Convert.ToUInt32(txtUniqueId.Text, 16);
                BitConverter.GetBytes(uniqueId).CopyTo(eepromData, EEPROM_UNIQUE_ID);

                // Write cell count
                eepromData[EEPROM_EXPECTED_CELL_COUNT] = (byte)numCellCount.Value;

                // Write max charge current (in units of 0.1A)
                ushort maxCharge = (ushort)(numMaxCharge.Value * 10);
                BitConverter.GetBytes(maxCharge).CopyTo(eepromData, EEPROM_MAX_CHARGE_CURRENT);

                // Write max discharge current
                ushort maxDischarge = (ushort)(numMaxDischarge.Value * 10);
                BitConverter.GetBytes(maxDischarge).CopyTo(eepromData, EEPROM_MAX_DISCHARGE_CURRENT);

                // Write count mismatch
                ushort countMismatch = (ushort)numCountMismatch.Value;
                BitConverter.GetBytes(countMismatch).CopyTo(eepromData, EEPROM_SEQUENTIAL_COUNT_MISMATCH);

                RefreshView();
                MessageBox.Show("Changes applied to memory", "Success", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error applying changes: {ex.Message}", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void RefreshView_Click(object sender, EventArgs e)
        {
            RefreshView();
        }
    }

    class Program
    {
        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new MainForm());
        }
    }
}