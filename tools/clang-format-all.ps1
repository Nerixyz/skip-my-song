$ok = Read-Host "Are you sure you want to run clang-format on all source files? (y/n)";
if ($ok -in "y", "yes", "yup", "yep") {
    Get-ChildItem src -Recurse -Include *.hpp, *.cpp | ForEach-Object -Parallel { clang-format -i "$($_.FullName)" };
}
