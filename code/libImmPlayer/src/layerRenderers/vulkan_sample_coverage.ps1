# Vulkan-only conversion: other backends retain their own coverage contracts.
function Convert-VulkanSampleCoverage([string]$Source) {
    $original = 'const int MSAASampleCount = 8;'
    if (-not $Source.Contains($original)) { throw 'Expected legacy coverage sample count was not found' }
    $Source = $Source.Replace($original, 'int MSAASampleCount = int(immTargetSampleCount);')
    $Source = [regex]::Replace($Source, 'uint mask = \(0xff00u? >> uint\(al\*float\(MSAASampleCount\) \+ 0\.5\)\) & 0xffu?;',
        'uint fullMask = (1u << immTargetSampleCount) - 1u; uint mask = (1u << uint(al*float(MSAASampleCount) + 0.5)) - 1u;')
    $Source = $Source.Replace('uint shift = uint(ran*7.0);', 'uint shift = uint(ran * float(MSAASampleCount));')
    $Source = [regex]::Replace($Source, 'shift &= 7u?;', 'shift %= immTargetSampleCount;')
    $Source = $Source.Replace('uint b = (mask << 8) | mask;', 'uint b = (mask << immTargetSampleCount) | mask;')
    $Source = [regex]::Replace($Source, 'mask = \(b >> shift\) & 0xffu?;', 'mask = (b >> shift) & fullMask;')
    return "layout(constant_id=1) const uint immTargetSampleCount = 8u;`n$Source"
}
