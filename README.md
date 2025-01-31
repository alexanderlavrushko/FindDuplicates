# FindDuplicates
Command line tool to find file duplicates in specified folders.

## Usage
The project is curently built with Visual Studio 2019. Output binary will appear in `FindDuplicateFiles\build\<PLATFORM>\<CONFIG>` folder.

Run it like this to scan one directory:

`FindDuplicateFiles.exe "C:\Alexander\test-duplicates"`

Or multiple directories:

`FindDuplicateFiles.exe "C:\Alexander\test-duplicates" "C:\Alexander\test-duplicates-2"`

Note: if files are too big, only first 1GB is compared.
