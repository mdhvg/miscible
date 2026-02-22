function Format-AnsiString {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Text,

        [Parameter(Mandatory=$true)]
        [int]$R,

        [Parameter(Mandatory=$true)]
        [int]$G,

        [Parameter(Mandatory=$true)]
        [int]$B,

        [switch]$Bold,
        [switch]$Italic,
        [switch]$Underline,
        [switch]$Strikethrough,
        [switch]$Reverse,
        [switch]$Dim
    )

    $esc = [char]27
    $codes = @()

    if ($Bold)         { $codes += "1" }
    if ($Dim)          { $codes += "2" }
    if ($Italic)       { $codes += "3" }
    if ($Underline)    { $codes += "4" }
    if ($Reverse)      { $codes += "7" }
    if ($Strikethrough){ $codes += "9" }

    $codes += "38;2;${R};${G};${B}"

    $ansiStart = "${esc}[" + ($codes -join ";") + "m"
    $ansiEnd   = "${esc}[0m"

    return "${ansiStart}${Text}${ansiEnd}"
}

