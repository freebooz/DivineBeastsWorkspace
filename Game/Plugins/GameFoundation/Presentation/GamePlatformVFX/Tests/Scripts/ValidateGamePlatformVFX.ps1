param(
    [Parameter(Mandatory=$true)][string]$UnrealEditorCmd,
    [Parameter(Mandatory=$true)][string]$Project
)

& $UnrealEditorCmd $Project -run=DataValidation -unattended -nop4
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
exit 0
