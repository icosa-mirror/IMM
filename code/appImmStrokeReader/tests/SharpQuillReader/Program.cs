if (args.Length > 1) throw new System.ArgumentException("Expected at most one Quill corpus directory.");
QuillReaderTests.Run(args.Length == 1 ? args[0] : null);
