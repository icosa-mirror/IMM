using System;
using System.Collections.Generic;

namespace ImmPlayer.Authoring
{
    public sealed class ImmAuthoringTransaction : IDisposable
    {
        private readonly ImmAuthoringDocument _source;
        private readonly HashSet<long> _affectedObjectIds = new HashSet<long>();
        private ImmAuthoringDocument _editable;
        private bool _completed;

        internal ImmAuthoringTransaction(
            ImmAuthoringDocument source,
            ImmAuthoringDocument editable,
            long baseRevision)
        {
            _source = source;
            _editable = editable;
            BaseRevision = baseRevision;
            _editable.Changed += RecordChange;
        }

        public long BaseRevision { get; }
        public bool IsCompleted => _completed;

        public ImmAuthoringDocument EditableDocument
        {
            get
            {
                if (_completed || _editable == null)
                    throw new ObjectDisposedException(nameof(ImmAuthoringTransaction));
                return _editable;
            }
        }

        public ImmAuthoringResult<long> Commit()
        {
            if (_completed || _editable == null)
                return ImmAuthoringResult<long>.Failure(ImmAuthoringErrorCode.Disposed, "Transaction is already completed.");

            long[] affected = new long[_affectedObjectIds.Count];
            _affectedObjectIds.CopyTo(affected);
            Array.Sort(affected);
            ImmAuthoringResult<long> result = _source.CommitTransaction(_editable, BaseRevision, affected);
            if (result.Succeeded)
                Complete();
            return result;
        }

        public void Abort() => Complete();

        public void Dispose()
        {
            Complete();
            GC.SuppressFinalize(this);
        }

        private void RecordChange(ImmAuthoringChange change)
        {
            foreach (long id in change.AffectedObjectIds)
                _affectedObjectIds.Add(id);
        }

        private void Complete()
        {
            if (_completed)
                return;
            _completed = true;
            if (_editable != null)
            {
                _editable.Changed -= RecordChange;
                _editable.Dispose();
                _editable = null;
            }
        }
    }
}
