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

        // Current floor from Shared.h - all currents are stored relative to this
        private const double CURRENT_FLOOR = -655.36;

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
        private Label lblModified;
        private RichTextBox txtHexView;
        private TabControl tabControl;
        private bool hasUnsavedChanges = false;

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
                Size = new Size(700, 20),
                BorderStyle = BorderStyle.Fixed3D
            };
            this.Controls.Add(lblFileName);

            // Modified indicator
            lblModified = new Label
            {
                Text = "",
                Location = new Point(720, 35),
                Size = new Size(150, 20),
                ForeColor = Color.Red,
                Font = new Font(this.Font, FontStyle.Bold)
            };
            this.Controls.Add(lblModified);

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
            txtUniqueId.TextChanged += (s, e) => MarkAsModified();
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
            numCellCount.ValueChanged += (s, e) => MarkAsModified();
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
                Minimum = (decimal)CURRENT_FLOOR,
                Maximum = 1000,
                DecimalPlaces = 2,
                Increment = 0.1m
            };
            numMaxCharge.ValueChanged += (s, e) => MarkAsModified();
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
                Minimum = (decimal)CURRENT_FLOOR,
                Maximum = 0,
                DecimalPlaces = 2,
                Increment = 0.1m
            };
            numMaxDischarge.ValueChanged += (s, e) => MarkAsModified();
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
                Maximum = 255  // Only 1 byte
            };
            numCountMismatch.ValueChanged += (s, e) => MarkAsModified();
            tab.Controls.Add(numCountMismatch);

            y += 50;

            // Info label
            var lblInfo = new Label
            {
                Text = string.Format("Metadata uses bytes 0x0000-0x003F ({0} bytes)\n" +
                       "Current usage: 11 bytes\n" +
                       "Available: {1} bytes", EEPROM_METADATA_SIZE, EEPROM_METADATA_SIZE - 11),
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
            // Auto-apply any pending changes before saving
            if (MessageBox.Show("Apply any pending changes before saving?", "Save EEPROM",
                MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes)
            {
                ApplyChanges_Click(sender, e);
            }

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
                lblFileName.Text = "File: " + Path.GetFileName(filename);
                RefreshView();
                ClearModified();
                MessageBox.Show("Loaded " + filename, "Success", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception ex)
            {
                MessageBox.Show("Error loading file: " + ex.Message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
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
                lblFileName.Text = "File: " + Path.GetFileName(filename) + " (saved)";
                MessageBox.Show("Saved to " + filename, "Success", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception ex)
            {
                MessageBox.Show("Error saving file: " + ex.Message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
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
            // Read unique ID (4 bytes, little-endian)
            uint uniqueId = (uint)(eepromData[EEPROM_UNIQUE_ID] |
                                  (eepromData[EEPROM_UNIQUE_ID + 1] << 8) |
                                  (eepromData[EEPROM_UNIQUE_ID + 2] << 16) |
                                  (eepromData[EEPROM_UNIQUE_ID + 3] << 24));
            txtUniqueId.Text = uniqueId.ToString("X8");

            // Read cell count (1 byte)
            numCellCount.Value = eepromData[EEPROM_EXPECTED_CELL_COUNT];

            // Read max charge current (2 bytes, little-endian)
            // Stored as units of 0.02A relative to CURRENT_FLOOR
            ushort maxChargeRaw = (ushort)(eepromData[EEPROM_MAX_CHARGE_CURRENT] |
                                           (eepromData[EEPROM_MAX_CHARGE_CURRENT + 1] << 8));
            double maxChargeAmps = CURRENT_FLOOR + (maxChargeRaw * 0.02);
            // Clamp to valid range for the control
            if (maxChargeAmps < (double)numMaxCharge.Minimum) maxChargeAmps = (double)numMaxCharge.Minimum;
            if (maxChargeAmps > (double)numMaxCharge.Maximum) maxChargeAmps = (double)numMaxCharge.Maximum;
            numMaxCharge.Value = (decimal)maxChargeAmps;

            // Read max discharge current (2 bytes, little-endian)
            // Stored as units of 0.02A relative to CURRENT_FLOOR
            ushort maxDischargeRaw = (ushort)(eepromData[EEPROM_MAX_DISCHARGE_CURRENT] |
                                              (eepromData[EEPROM_MAX_DISCHARGE_CURRENT + 1] << 8));
            double maxDischargeAmps = CURRENT_FLOOR + (maxDischargeRaw * 0.02);
            // Clamp to valid range for the control
            if (maxDischargeAmps < (double)numMaxDischarge.Minimum) maxDischargeAmps = (double)numMaxDischarge.Minimum;
            if (maxDischargeAmps > (double)numMaxDischarge.Maximum) maxDischargeAmps = (double)numMaxDischarge.Maximum;
            numMaxDischarge.Value = (decimal)maxDischargeAmps;

            // Read count mismatch (1 byte only!)
            byte countMismatch = eepromData[EEPROM_SEQUENTIAL_COUNT_MISMATCH];
            numCountMismatch.Value = countMismatch;
        }

        private void RefreshFrameCounterView()
        {
            dgvFrameCounters.Rows.Clear();

            uint maxCounter = 0;
            int maxPosition = -1;

            for (int pos = 0; pos < COUNTER_POSITIONS; pos++)
            {
                int addr = EEPROM_FRAME_COUNTER_BASE + (pos * BYTES_PER_COUNTER);

                // Read counter value (MSB first as per FRAMECOUNTER.c)
                uint value = (uint)eepromData[addr] << 24;
                value |= (uint)eepromData[addr + 1] << 16;
                value |= (uint)eepromData[addr + 2] << 8;
                value |= (uint)eepromData[addr + 3];

                string status = "";
                if (value == 0xFFFFFFFF)
                {
                    status = "Unprogrammed/Invalid";
                }
                else if (value > maxCounter && value != 0xFFFFFFFF)
                {
                    maxCounter = value;
                    maxPosition = pos;
                    status = "← Current (highest value)";
                }

                dgvFrameCounters.Rows.Add(
                    pos.ToString(),
                    string.Format("0x{0:X4}", addr),
                    value == 0xFFFFFFFF ? "INVALID" : value.ToString(),
                    string.Format("{0:X8}", value),
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
        }

        private void ApplyChanges_Click(object sender, EventArgs e)
        {
            try
            {
                // Write unique ID (little-endian)
                uint uniqueId = Convert.ToUInt32(txtUniqueId.Text, 16);
                eepromData[EEPROM_UNIQUE_ID] = (byte)(uniqueId & 0xFF);
                eepromData[EEPROM_UNIQUE_ID + 1] = (byte)((uniqueId >> 8) & 0xFF);
                eepromData[EEPROM_UNIQUE_ID + 2] = (byte)((uniqueId >> 16) & 0xFF);
                eepromData[EEPROM_UNIQUE_ID + 3] = (byte)((uniqueId >> 24) & 0xFF);

                // Write cell count
                eepromData[EEPROM_EXPECTED_CELL_COUNT] = (byte)numCellCount.Value;

                // Write max charge current
                // Convert from amps to units of 0.02A relative to CURRENT_FLOOR
                double maxChargeAmps = (double)numMaxCharge.Value;
                double maxChargeUnits = (maxChargeAmps - CURRENT_FLOOR) / 0.02;
                ushort maxChargeRaw = (ushort)maxChargeUnits;
                eepromData[EEPROM_MAX_CHARGE_CURRENT] = (byte)(maxChargeRaw & 0xFF);
                eepromData[EEPROM_MAX_CHARGE_CURRENT + 1] = (byte)((maxChargeRaw >> 8) & 0xFF);

                // Write max discharge current
                // Convert from amps to units of 0.02A relative to CURRENT_FLOOR
                double maxDischargeAmps = (double)numMaxDischarge.Value;
                double maxDischargeUnits = (maxDischargeAmps - CURRENT_FLOOR) / 0.02;
                ushort maxDischargeRaw = (ushort)maxDischargeUnits;
                eepromData[EEPROM_MAX_DISCHARGE_CURRENT] = (byte)(maxDischargeRaw & 0xFF);
                eepromData[EEPROM_MAX_DISCHARGE_CURRENT + 1] = (byte)((maxDischargeRaw >> 8) & 0xFF);

                // Write count mismatch (only 1 byte!)
                byte countMismatch = (byte)numCountMismatch.Value;
                eepromData[EEPROM_SEQUENTIAL_COUNT_MISMATCH] = countMismatch;

                RefreshView();
                ClearModified();
                MessageBox.Show("Changes applied to memory", "Success", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception ex)
            {
                MessageBox.Show("Error applying changes: " + ex.Message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void RefreshView_Click(object sender, EventArgs e)
        {
            RefreshView();
        }

        private void MarkAsModified()
        {
            if (!hasUnsavedChanges)
            {
                hasUnsavedChanges = true;
                lblModified.Text = "* CHANGES NOT APPLIED";
            }
        }

        private void ClearModified()
        {
            hasUnsavedChanges = false;
            lblModified.Text = "";
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