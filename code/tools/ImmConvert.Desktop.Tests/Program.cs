using System;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using ImmPlayer.Conversion;
using ImmPlayer.Desktop;

internal static class Program
{
    private static void Require(bool value, string message)
    { if (!value) throw new InvalidOperationException(message); }
    private static T Find<T>(Control form, string name) where T : Control => (T)form.Controls.Find(name, true).Single();
    private static async Task WaitFor(Func<bool> predicate)
    {
        DateTime deadline = DateTime.UtcNow.AddSeconds(30);
        while (!predicate())
        {
            if (DateTime.UtcNow >= deadline) throw new TimeoutException("Desktop conversion did not settle");
            await Task.Delay(10);
        }
    }

    [STAThread]
    private static int Main(string[] args)
    {
        Application.SetHighDpiMode(HighDpiMode.PerMonitorV2);
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        string output = Path.GetFullPath(args[0]);
        string source = Path.GetFullPath(args[1]);
        string native = Path.GetFullPath(args[2]);
        Directory.CreateDirectory(output);
        string report = Path.Combine(output, "report.json");
        File.WriteAllText(report, "{\"passed\":false}");
        using var release = new ManualResetEventSlim(false);
        using var entered = new ManualResetEventSlim(false);
        using var form = new ConverterForm(native, (request, token, progress) =>
        {
            if (Path.GetFileName(request.Output).StartsWith("cancel", StringComparison.Ordinal))
            {
                entered.Set();
                if (!release.Wait(TimeSpan.FromSeconds(15))) throw new TimeoutException("Test gate was not released");
            }
            return StandaloneConversion.Run(request, token, progress);
        });
        int exitCode = 1;
        bool testingClose = false;
        string closeOutput = Path.Combine(output, "cancel-close.imm");
        form.FormClosed += (sender, eventArgs) =>
        {
            if (!testingClose) return;
            try
            {
                Require(File.ReadAllText(closeOutput) == "keep output on close", "Closing form replaced existing output");
                Require(Directory.GetFiles(output, ".imm-convert-*.imm").Length == 0, "Form left staging files");
                File.WriteAllText(report, JsonSerializer.Serialize(new { passed = true, cases = 8,
                    frontendAssembly = typeof(ConverterForm).Assembly.Location,
                    hostAssembly = typeof(StandaloneConversion).Assembly.Location,
                    windowsFormsAssembly = typeof(Form).Assembly.Location,
                    runtimeDirectory = RuntimeEnvironment.GetRuntimeDirectory(),
                    scope = "real WinForms event-loop/native conversion and controlled pre-native cancellation/close; live input/Unity acceptance separate" }));
                exitCode = 0;
                Console.WriteLine("[IMM_CONVERT_DESKTOP_TEST] conversion, repeat, refusal, partial output, cancellation, recovery and close passed");
            }
            catch (Exception exception) { Console.Error.WriteLine(exception); }
        };
        form.Shown += async (sender, eventArgs) =>
        {
            try
            {
                var project = Find<TextBox>(form, "projectPath");
                var destination = Find<TextBox>(form, "outputPath");
                var convert = Find<Button>(form, "convert");
                var cancel = Find<Button>(form, "cancel");
                var overwrite = Find<CheckBox>(form, "overwrite");
                var status = Find<Label>(form, "status");
                var settings = Find<CheckBox>(form, "allowOmissions");
                Require(!settings.Checked && !overwrite.Checked, "Omission/overwrite defaults must be strict");
                project.Text = source;
                destination.Text = Path.Combine(output, "first.imm");
                convert.PerformClick();
                Require(!convert.Enabled && cancel.Enabled && !project.Enabled, "Conversion did not disable inputs/enable cancellation");
                await WaitFor(() => convert.Enabled);
                Require(File.Exists(destination.Text) && status.Text == "Conversion complete.", "Form did not publish complete output");
                Require(Find<Button>(form, "showOutput").Enabled, "Published output is not available");
                byte[] original = File.ReadAllBytes(destination.Text);
                convert.PerformClick();
                Require(status.Text == "Check the project and output settings." && File.ReadAllBytes(destination.Text).SequenceEqual(original), "Existing output was not protected");

                destination.Text = Path.Combine(output, "repeat.imm");
                convert.PerformClick();
                await WaitFor(() => convert.Enabled);
                Require(File.Exists(destination.Text) && status.Text == "Conversion complete.", "Repeated form conversion failed");
                Require(Find<ProgressBar>(form, "progress").Value == 100, "Published output did not complete progress");
                await Task.Delay(1100);
                using (var bitmap = new Bitmap(form.Width, form.Height))
                {
                    form.DrawToBitmap(bitmap, new Rectangle(Point.Empty, form.Size));
                    bitmap.Save(Path.Combine(output, "completed-form.png"));
                }
                string partial = Path.Combine(output, "partial project");
                Directory.CreateDirectory(partial);
                File.Copy(Path.Combine(source, "Quill.qbin"), Path.Combine(partial, "Quill.qbin"));
                var json = JsonNode.Parse(File.ReadAllText(Path.Combine(source, "Quill.json")));
                var children = json["Sequence"]["RootLayer"]["Implementation"]["Children"].AsArray();
                var model = children[0].DeepClone();
                model["Name"] = "Unsupported model"; model["Type"] = "Model"; model["Implementation"] = new JsonObject();
                children.Add(model);
                File.WriteAllText(Path.Combine(partial, "Quill.json"), json.ToJsonString());
                project.Text = partial;
                destination.Text = Path.Combine(output, "partial.imm");
                convert.PerformClick();
                await WaitFor(() => convert.Enabled);
                Require(!File.Exists(destination.Text) && status.Text.StartsWith("Conversion failed"), "Unsupported content was silently omitted");
                settings.Checked = true;
                convert.PerformClick();
                await WaitFor(() => convert.Enabled);
                Require(File.Exists(destination.Text) && status.Text.StartsWith("Converted with omissions"), "Explicit partial conversion was not labelled");
                project.Text = source;
                settings.Checked = false;
                overwrite.Checked = true;
                destination.Text = Path.Combine(output, "cancel.imm");
                File.WriteAllText(destination.Text, "keep existing output");
                convert.PerformClick();
                await WaitFor(() => entered.IsSet);
                cancel.PerformClick();
                Require(!cancel.Enabled && status.Text.StartsWith("Cancelling"), "Cancellation was not requested through the form");
                release.Set();
                await WaitFor(() => convert.Enabled);
                Require(status.Text.StartsWith("Conversion cancelled") && File.ReadAllText(destination.Text) == "keep existing output", "Cancelled form conversion replaced output");

                destination.Text = Path.Combine(output, "recovered.imm");
                convert.PerformClick();
                await WaitFor(() => convert.Enabled);
                Require(File.Exists(destination.Text) && status.Text == "Conversion complete.", "Form did not recover after cancellation");
                form.Size = form.MinimumSize;
                await Task.Delay(1100);
                var layout = (TableLayoutPanel)form.Controls[0];
                File.WriteAllText(Path.Combine(output, "minimum-layout.json"), JsonSerializer.Serialize(new {
                    form.ClientSize.Width, form.ClientSize.Height, rows = layout.GetRowHeights(),
                    controls = layout.Controls.Cast<Control>().Select(control => new {
                        type = control.GetType().Name, control.Text, control.Top, control.Height,
                        preferredHeight = control.PreferredSize.Height }).ToArray() }));
                using (var bitmap = new Bitmap(form.Width, form.Height))
                {
                    form.DrawToBitmap(bitmap, new Rectangle(Point.Empty, form.Size));
                    bitmap.Save(Path.Combine(output, "minimum-form.png"));
                }
                Require(Find<DataGridView>(form, "details").Height >= 60, "Minimum window hides conversion details");
                Require(Find<DataGridView>(form, "details").Parent.RectangleToScreen(
                    Find<DataGridView>(form, "details").Parent.ClientRectangle).Bottom <=
                    convert.Parent.RectangleToScreen(convert.Parent.ClientRectangle).Top,
                    "Conversion details overlap the action buttons");
                Require(form.RectangleToClient(convert.RectangleToScreen(convert.ClientRectangle)).Bottom <= form.ClientSize.Height,
                    "Minimum window hides the Convert button");
                release.Reset(); entered.Reset();
                destination.Text = closeOutput;
                File.WriteAllText(closeOutput, "keep output on close");
                convert.PerformClick();
                await WaitFor(() => entered.IsSet);
                form.Close();
                Require(!form.IsDisposed && !cancel.Enabled, "Form closed before requesting worker cancellation");
                testingClose = true;
                release.Set();
            }
            catch (Exception exception) { Console.Error.WriteLine(exception); }
            finally { release.Set(); if (!testingClose) form.Close(); }
        };
        Application.Run(form);
        return exitCode;
    }
}
