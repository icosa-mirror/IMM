using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using ImmPlayer.Authoring;
using ImmPlayer.Conversion;

namespace ImmPlayer.Desktop
{
    internal sealed class ConverterForm : Form
    {
        private readonly TextBox source = new TextBox { Name = "projectPath", AccessibleName = "Quill project folder", Dock = DockStyle.Fill };
        private readonly TextBox output = new TextBox { Name = "outputPath", AccessibleName = "IMM output file", Dock = DockStyle.Fill };
        private readonly CheckBox omissions = new CheckBox { Name = "allowOmissions", Text = "Allow partial conversion (omit unsupported content)", AutoSize = true };
        private readonly CheckBox overwrite = new CheckBox { Name = "overwrite", Text = "Replace existing IMM output", AutoSize = true };
        private readonly CheckBox saveReport = new CheckBox { Name = "saveReport", Text = "Save a conversion report beside the IMM", Checked = true, AutoSize = true };
        private readonly NumericUpDown limit = new NumericUpDown { Name = "inputLimit", Minimum = 1, Maximum = 4096, Value = 256, Width = 90 };
        private readonly Button convert = new Button { Name = "convert", Text = "Convert", AutoSize = true, MinimumSize = new Size(110, 36) };
        private readonly Button cancel = new Button { Name = "cancel", Text = "Cancel", Enabled = false, AutoSize = true, MinimumSize = new Size(90, 36) };
        private readonly Button showOutput = new Button { Name = "showOutput", Text = "Show output folder", Enabled = false, AutoSize = true, MinimumSize = new Size(130, 36) };
        private readonly Label status = new Label { Name = "status", Text = "Choose a Quill project to begin.", AutoSize = true, Dock = DockStyle.Fill };
        private readonly ProgressBar bar = new ProgressBar { Name = "progress", Dock = DockStyle.Fill, Height = 18 };
        private readonly DataGridView details = new DataGridView { Name = "details", Dock = DockStyle.Fill, ReadOnly = true,
            AllowUserToAddRows = false, AllowUserToDeleteRows = false, RowHeadersVisible = false,
            BackgroundColor = SystemColors.Window, AutoSizeRowsMode = DataGridViewAutoSizeRowsMode.AllCells,
            SelectionMode = DataGridViewSelectionMode.FullRowSelect, MultiSelect = true };
        private readonly GroupBox settings = new GroupBox { Text = "Project and output", AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, Dock = DockStyle.Fill, Padding = new Padding(12) };
        private readonly string nativeDirectory;
        private readonly Func<StandaloneConversionRequest, CancellationToken, IProgress<ImmAuthoringProgress>, StandaloneConversionOutcome> runConversion;
        private string suggestedOutput;
        private string lastPublishedOutput;
        private CancellationTokenSource cancellation;
        private readonly System.Windows.Forms.Timer progressTimer = new System.Windows.Forms.Timer { Interval = 100 };
        private LatestProgress latestProgress;
        private bool running;
        private bool closeWhenFinished;

        public ConverterForm(string nativeDirectory = null,
            Func<StandaloneConversionRequest, CancellationToken, IProgress<ImmAuthoringProgress>, StandaloneConversionOutcome> runConversion = null)
        {
            this.nativeDirectory = nativeDirectory;
            this.runConversion = runConversion ?? StandaloneConversion.Run;
            Text = "Quill to IMM Converter";
            Name = "converterWindow";
            Font = new Font("Segoe UI", 10);
            AutoScaleDimensions = new SizeF(96, 96);
            AutoScaleMode = AutoScaleMode.Dpi;
            StartPosition = FormStartPosition.CenterScreen;
            MinimumSize = new Size(760, 680);
            ClientSize = new Size(880, 660);
            var main = new TableLayoutPanel { Dock = DockStyle.Fill, Padding = new Padding(20), ColumnCount = 1, RowCount = 7 };
            main.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            for (int i = 0; i < 7; i++) main.RowStyles.Add(new RowStyle(i == 4 ? SizeType.Percent : SizeType.AutoSize, i == 4 ? 100 : 0));
            main.Controls.Add(new Label { Text = "Quill to IMM", Font = new Font(Font.FontFamily, 22, FontStyle.Bold), AutoSize = true }, 0, 0);
            main.Controls.Add(new Label { Text = "Select a project folder containing Quill.json and Quill.qbin. You can also drop the folder below.",
                AutoSize = true, MaximumSize = new Size(800, 0), Margin = new Padding(3, 4, 3, 16) }, 0, 1);
            BuildSettings();
            main.Controls.Add(settings, 0, 2);
            var progress = new TableLayoutPanel { AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, Dock = DockStyle.Fill, ColumnCount = 1, Padding = new Padding(0, 12, 0, 10) };
            progress.Controls.Add(status);
            progress.Controls.Add(bar);
            main.Controls.Add(progress, 0, 3);
            details.DefaultCellStyle.WrapMode = DataGridViewTriState.True;
            details.Columns.Add(new DataGridViewTextBoxColumn { HeaderText = "Level", Width = 85 });
            details.Columns.Add(new DataGridViewTextBoxColumn { HeaderText = "Location", Width = 200 });
            details.Columns.Add(new DataGridViewTextBoxColumn { HeaderText = "Details", AutoSizeMode = DataGridViewAutoSizeColumnMode.Fill });
            var detailGroup = new GroupBox { Text = "Conversion details", Dock = DockStyle.Fill, Padding = new Padding(10), MinimumSize = new Size(0, 100) };
            detailGroup.Controls.Add(details);
            main.Controls.Add(detailGroup, 0, 4);
            var actions = new FlowLayoutPanel { Dock = DockStyle.Fill, AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, FlowDirection = FlowDirection.RightToLeft, Padding = new Padding(0, 12, 0, 6) };
            actions.Controls.Add(convert);
            actions.Controls.Add(cancel);
            actions.Controls.Add(showOutput);
            main.Controls.Add(actions, 0, 5);
            main.Controls.Add(new Label { Text = "Unsupported content is rejected by default. Review any omissions before using a partial conversion.",
                AutoSize = true, ForeColor = SystemColors.GrayText, MaximumSize = new Size(800, 0) }, 0, 6);
            Controls.Add(main);
            AcceptButton = convert;
            source.TextChanged += (sender, args) => SuggestOutput();
            output.TextChanged += (sender, args) => ClearResult();
            omissions.CheckedChanged += (sender, args) => ClearResult();
            overwrite.CheckedChanged += (sender, args) => ClearResult();
            saveReport.CheckedChanged += (sender, args) => ClearResult();
            limit.ValueChanged += (sender, args) => ClearResult();
            convert.Click += async (sender, args) => await ConvertAsync();
            cancel.Click += (sender, args) => RequestCancellation();
            progressTimer.Tick += (sender, args) =>
            {
                if (latestProgress != null && latestProgress.TryTake(out var value)) UpdateProgress(value);
            };
            showOutput.Click += (sender, args) => ShowOutput();
            FormClosing += (sender, args) =>
            {
                if (!running) return;
                args.Cancel = true;
                closeWhenFinished = true;
                RequestCancellation();
            };
            source.AllowDrop = true;
            source.DragEnter += (sender, args) =>
            {
                if (args.Data?.GetData(DataFormats.FileDrop) is string[] paths && paths.Length == 1 && Directory.Exists(paths[0]))
                    args.Effect = DragDropEffects.Copy;
            };
            source.DragDrop += (sender, args) =>
            {
                if (args.Data?.GetData(DataFormats.FileDrop) is string[] paths && paths.Length == 1) source.Text = paths[0];
            };
        }

        private void BuildSettings()
        {
            var table = new TableLayoutPanel { AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, Dock = DockStyle.Fill, ColumnCount = 2 };
            table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            table.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            var sourceLabel = new Label { Text = "Quill project folder", AutoSize = true };
            table.Controls.Add(sourceLabel, 0, 0); table.SetColumnSpan(sourceLabel, 2);
            table.Controls.Add(source, 0, 1);
            var browseSource = new Button { Text = "Browse…", Name = "browseProject", AutoSize = true };
            browseSource.Click += (sender, args) =>
            {
                using var dialog = new FolderBrowserDialog { Description = "Choose the Quill project folder", UseDescriptionForTitle = true, ShowNewFolderButton = false };
                if (Directory.Exists(source.Text)) dialog.SelectedPath = source.Text;
                if (dialog.ShowDialog(this) == DialogResult.OK) source.Text = dialog.SelectedPath;
            };
            table.Controls.Add(browseSource, 1, 1);
            var outputLabel = new Label { Text = "IMM output file", AutoSize = true, Margin = new Padding(3, 12, 3, 3) };
            table.Controls.Add(outputLabel, 0, 2); table.SetColumnSpan(outputLabel, 2);
            table.Controls.Add(output, 0, 3);
            var browseOutput = new Button { Text = "Browse…", Name = "browseOutput", AutoSize = true };
            browseOutput.Click += (sender, args) =>
            {
                using var dialog = new SaveFileDialog { Title = "Choose IMM output", Filter = "IMM files (*.imm)|*.imm", DefaultExt = "imm", AddExtension = true, OverwritePrompt = false };
                if (!string.IsNullOrWhiteSpace(output.Text)) dialog.FileName = output.Text;
                if (dialog.ShowDialog(this) == DialogResult.OK) output.Text = dialog.FileName;
            };
            table.Controls.Add(browseOutput, 1, 3);
            var options = new TableLayoutPanel { AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, Dock = DockStyle.Top,
                ColumnCount = 2, RowCount = 2, Padding = new Padding(0, 10, 0, 0) };
            options.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 60));
            options.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 40));
            options.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            options.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            options.Controls.Add(omissions, 0, 0); options.Controls.Add(overwrite, 1, 0); options.Controls.Add(saveReport, 0, 1);
            var limits = new FlowLayoutPanel { AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, WrapContents = false, Padding = new Padding(0, 4, 0, 0) };
            limits.Controls.Add(new Label { Text = "Input size limit (MiB)", AutoSize = true, Margin = new Padding(3, 5, 6, 3) });
            limits.Controls.Add(limit);
            options.Controls.Add(limits, 1, 1);
            table.Controls.Add(options, 0, 4); table.SetColumnSpan(options, 2);
            settings.Controls.Add(table);
        }

        private void SuggestOutput()
        {
            if (running) return;
            try
            {
                if (!string.IsNullOrWhiteSpace(source.Text))
                {
                    string folder = Path.TrimEndingDirectorySeparator(Path.GetFullPath(source.Text));
                    string next = Path.Combine(Path.GetDirectoryName(folder) ?? folder, $"{Path.GetFileName(folder)}.imm");
                    if (string.IsNullOrWhiteSpace(output.Text) || output.Text == suggestedOutput) output.Text = next;
                    suggestedOutput = next;
                }
            }
            catch (Exception exception) when (exception is ArgumentException || exception is NotSupportedException || exception is PathTooLongException) { }
            ClearResult();
        }

        private void ClearResult()
        {
            if (running) return;
            details.Rows.Clear();
            status.Text = "Ready to convert.";
            status.ForeColor = SystemColors.ControlText;
            showOutput.Enabled = false;
            bar.Value = 0;
        }

        private async Task ConvertAsync()
        {
            if (running) return;
            StandaloneConversionRequest request;
            try
            {
                string report = saveReport.Checked && !string.IsNullOrWhiteSpace(output.Text)
                    ? Path.ChangeExtension(Path.GetFullPath(output.Text), ".conversion.json") : null;
                request = new StandaloneConversionRequest(source.Text, output.Text, omissions.Checked, overwrite.Checked,
                    report, nativeDirectory, checked((long)limit.Value * ImmAuthoringLimits.MiB),
                    typeof(ConverterForm).Assembly.GetCustomAttribute<AssemblyInformationalVersionAttribute>()?.InformationalVersion);
                StandaloneConversion.Validate(request);
            }
            catch (Exception exception) when (exception is ArgumentException || exception is NotSupportedException || exception is PathTooLongException)
            { ShowError("Check the project and output settings.", exception.Message); return; }
            details.Rows.Clear();
            running = true;
            settings.Enabled = false;
            convert.Enabled = false;
            cancel.Enabled = true;
            showOutput.Enabled = false;
            status.ForeColor = SystemColors.ControlText;
            status.Text = "Starting conversion…";
            bar.Value = 0;
            cancellation = new CancellationTokenSource();
            // The compiler reports per stroke. Keep only the latest value and
            // paint it at UI cadence instead of queuing a UI callback per stroke.
            latestProgress = new LatestProgress();
            progressTimer.Start();
            try
            {
                CancellationToken token = cancellation.Token;
                var progress = latestProgress;
                var result = await Task.Run(() => runConversion(request, token, progress));
                foreach (var issue in result.Conversion?.Issues ?? Array.Empty<QuillToImmIssue>())
                    details.Rows.Add(issue.Severity.ToString(), issue.SourcePath, issue.Message);
                if (result.Error != null) details.Rows.Add("Error", "", result.Error);
                if (result.ReportError != null) details.Rows.Add("Error", "Report", result.ReportError);
                if (result.CleanupError != null) details.Rows.Add("Warning", "", result.CleanupError);
                if (result.OutputPublished)
                {
                    bar.Value = 100;
                    status.Text = result.ReportError != null ? "IMM saved, but its report could not be written." :
                        result.IsPartial ? "Converted with omissions. Review the details below." : "Conversion complete.";
                    status.ForeColor = result.IsPartial || result.ReportError != null ? Color.DarkOrange : Color.DarkGreen;
                    lastPublishedOutput = request.Output;
                    showOutput.Enabled = true;
                }
                else
                {
                    status.Text = result.ExitCode == 130 ? "Conversion cancelled. Existing output was kept." : "Conversion failed. Existing output was kept.";
                    status.ForeColor = result.ExitCode == 130 ? SystemColors.ControlText : Color.Firebrick;
                }
            }
            catch (Exception exception) { ShowError("Conversion failed.", exception.Message); }
            finally
            {
                running = false;
                progressTimer.Stop(); latestProgress = null;
                settings.Enabled = true;
                convert.Enabled = true;
                cancel.Enabled = false;
                cancellation.Dispose(); cancellation = null;
                if (closeWhenFinished) BeginInvoke(new Action(Close));
            }
        }

        private void UpdateProgress(ImmAuthoringProgress progress)
        {
            if (!running || IsDisposed || cancellation?.IsCancellationRequested == true) return;
            status.Text = progress.Stage switch
            {
                ImmAuthoringProgressStage.Validating => "Checking project…",
                ImmAuthoringProgressStage.CompilingGraph => "Preparing scene…",
                ImmAuthoringProgressStage.Serializing => "Building IMM…",
                ImmAuthoringProgressStage.WritingOutput => "Saving IMM…",
                ImmAuthoringProgressStage.Completed => "Finishing conversion…",
                _ => "Reading project…"
            };
            int start = progress.Stage switch { ImmAuthoringProgressStage.CompilingGraph => 15,
                ImmAuthoringProgressStage.Serializing => 75, ImmAuthoringProgressStage.WritingOutput => 90,
                ImmAuthoringProgressStage.Completed => 99, _ => 0 };
            int width = progress.Stage == ImmAuthoringProgressStage.CompilingGraph ? 60 : progress.Stage == ImmAuthoringProgressStage.Validating ? 10 : 5;
            bar.Value = Math.Max(bar.Value, Math.Clamp(start + (int)(progress.Fraction * width), 0, 99));
        }

        private void RequestCancellation()
        {
            if (!running || cancellation == null) return;
            cancellation.Cancel();
            cancel.Enabled = false;
            status.Text = "Cancelling when the current step finishes…";
        }

        private void ShowError(string summary, string message)
        {
            status.Text = summary;
            status.ForeColor = Color.Firebrick;
            details.Rows.Clear(); details.Rows.Add("Error", "", message);
        }

        private void ShowOutput()
        {
            if (lastPublishedOutput == null || !File.Exists(lastPublishedOutput)) return;
            try
            {
                var start = new ProcessStartInfo("explorer.exe") { UseShellExecute = true };
                start.ArgumentList.Add($"/select,{lastPublishedOutput}");
                Process.Start(start);
            }
            catch (Exception exception) { ShowError("Could not open the output folder.", exception.Message); }
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing) progressTimer.Dispose();
            base.Dispose(disposing);
        }

        private sealed class LatestProgress : IProgress<ImmAuthoringProgress>
        {
            private readonly object gate = new object();
            private ImmAuthoringProgress value;
            private bool changed;
            public void Report(ImmAuthoringProgress progress)
            { lock (gate) { value = progress; changed = true; } }
            public bool TryTake(out ImmAuthoringProgress progress)
            { lock (gate) { progress = value; bool result = changed; changed = false; return result; } }
        }
    }
}
