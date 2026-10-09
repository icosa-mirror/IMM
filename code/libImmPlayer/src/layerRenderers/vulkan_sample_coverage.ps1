# Vulkan-only conversion: other backends retain their own coverage contracts.
function Convert-VulkanSampleCoverage([string]$Source) {
    $original = 'const int MSAASampleCount = 8;'
    if (-not $Source.Contains($original)) { throw 'Expected legacy coverage sample count was not found' }
    $Source = $Source.Replace($original, 'int MSAASampleCount = int(immTargetSampleCount);')
    $Source = $Source.Replace('uint mask = (0xff00 >> uint(al*float(MSAASampleCount) + 0.5)) & 0xff;',
        'uint fullMask = (1u << immTargetSampleCount) - 1u; uint mask = (1u << uint(al*float(MSAASampleCount) + 0.5)) - 1u;')
    $Source = $Source.Replace('uint shift = uint(ran*7.0);', 'uint shift = uint(ran * float(MSAASampleCount));')
    $Source = $Source.Replace('shift &= 7;', 'shift %= immTargetSampleCount;')
    $Source = $Source.Replace('uint b = (mask << 8) | mask;', 'uint b = (mask << immTargetSampleCount) | mask;')
    $Source = $Source.Replace('mask = (b >> shift) & 0xff;', 'mask = (b >> shift) & fullMask;')
    return "layout(constant_id=1) const uint immTargetSampleCount = 8u;`n$Source"
}
