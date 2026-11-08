function Get-ActiveUnrealFiles {
    param([Parameter(Mandatory)][string]$Root, [Parameter(Mandatory)][string]$Filter)
    if (-not (Test-Path -LiteralPath $Root -PathType Container)) { return }
    $excluded = @('Saved','Intermediate','Binaries','DerivedDataCache','Worktrees','.worktrees','.git')
    $directories = [System.Collections.Generic.Stack[IO.DirectoryInfo]]::new()
    $directories.Push([IO.DirectoryInfo]::new($Root))
    while ($directories.Count) {
        $directory = $directories.Pop()
        $directory.EnumerateFiles($Filter) | Where-Object Name -like $Filter
        foreach ($child in $directory.EnumerateDirectories()) {
            if ($child.Name -notin $excluded -and -not ($child.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
                $directories.Push($child)
            }
        }
    }
}
