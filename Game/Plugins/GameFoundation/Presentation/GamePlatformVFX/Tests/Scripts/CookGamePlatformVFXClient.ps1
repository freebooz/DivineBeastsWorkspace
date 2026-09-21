param(
    [Parameter(Mandatory=$true)][string]$RunUAT,
    [Parameter(Mandatory=$true)][string]$Project,
    [string]$Platform="Win64"
)

& $RunUAT BuildCookRun -project=$Project -noP4 -clientconfig=Development -platform=$Platform -build -cook -stage -pak -utf8output
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
exit 0
