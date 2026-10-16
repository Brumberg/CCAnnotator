# CCAnnotator

CCAnnotator loads a code coverage project and annotates the generated HTML pages based on comments in the source code.

## Create ctags

To create the ctags file, use a command such as:

```powershell
ctags --fields=+ne --c-kinds=f --output-format=json -f ./tag AcqData.h AcqData.cpp
```

This process is automated by the command-line script `create_tags.cmd`. You only have to specify the required source files in the script.

## Loading the project and subprojects

After cloning the repository without submodules, initialize and update the submodules using:

```powershell
git submodule update --init --recursive
```

The submodules may be removed in future releases, as they are bypassed by the current implementation.

## Enable code coverage with Clang

Enabling code coverage with `clang-cl` can be tricky.

Specify the following flags in the compiler settings:

```text
-fprofile-instr-generate -fcoverage-mapping
```

## Create the coverage files

### Step 1: Generate the raw profile

Run the instrumented test executable from the root project directory:

```powershell
$env:LLVM_PROFILE_FILE="coverage.profraw"
& ".\x64\Unit Test - Release\P300_SignalProcessing.exe"
```

This creates the raw profile file:

```text
coverage.profraw
```

### Step 2: Create the merged profile data

Convert the raw profile data into a `.profdata` file:

```powershell
llvm-profdata merge -sparse coverage.profraw -o coverage.profdata
```

If `llvm-profdata` is not in your `PATH`, specify its full path. For example, in PowerShell:

```powershell
$llvm = "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\Llvm\x64\bin"

& "$llvm\llvm-profdata.exe" merge `
    -sparse `
    coverage.profraw `
    -o coverage.profdata
```

### Step 3: Create the coverage report

To display a summary of the collected coverage data, use:

```powershell
llvm-cov report `
    ".\x64\Unit Test - Release\P300_SignalProcessing.exe" `
    -instr-profile=".\coverage.profdata"
```

> **Hint:**  
> If `llvm-cov` cannot be found, add the LLVM `bin` directory to your `PATH`:
>
> ```powershell
> $env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\Llvm\x64\bin"
> ```

The report contains information such as:

```text
Filename
Regions
Missed Regions
Cover
Functions
Missed Functions
Executed
Lines
Missed Lines
Cover
Branches
Missed Branches
Cover
```

To generate an HTML coverage report for a specific source file:

```powershell
llvm-cov show `
    ".\P300_SignalProcessing.exe" `
    -instr-profile=".\coverage.profdata" `
    -format=html `
    -output-dir="coverage_ProcessData" `
    "ProcessData.cpp"
```

For multiple source files:

```powershell
llvm-cov show `
    ".\P300_SignalProcessing.exe" `
    -instr-profile=".\coverage.profdata" `
    -format=html `
    -output-dir="coverage_report" `
    "ProcessData.cpp" `
    "OtherFile.cpp"
```

Header files containing template or inline implementations can also be included:

```powershell
llvm-cov show `
    ".\P300_SignalProcessing.exe" `
    -instr-profile=".\coverage.profdata" `
    -format=html `
    -output-dir="coverage_report" `
    "carray.h" `
    "dsparray.h" `
    "ProcessData.cpp"
```

For example, using the unit test executable:

```powershell
llvm-cov show `
    ".\x64\Unit Test - Release\P300_SignalProcessing.exe" `
    -instr-profile=".\coverage.profdata" `
    -format=html `
    -output-dir="coverage_report" `
    "carray.h" `
    "DSPArray.h" `
    "FFT.h" `
    "mathematics.h" `
    "ProcessData.cpp"
```

### Step 4: Annotate the coverage report

Copy the resulting coverage files into the CCAnnotator folder.

Then run `ccannotator.exe` and specify the generated `index.html` file and coverage report directory. For example:

```powershell
ccannotator.exe `
    --index_file=index.html `

### Step 5: Identify/demangle template instantiations

Template instantiations in the coverage report appear as MSVC-mangled symbol names. The `undname` utility can be used to convert these symbols back into readable C++ declarations.

For example, given the following symbol from the HTML coverage report:

```text
??$append@F$0DC@@?$CArray@F$0BO@@@QEAA?AViterator@0@AEAV?$CArray@F$0DC@@@@Z
```

Run:

```powershell
undname '??$append@F$0DC@@?$CArray@F$0BO@@@QEAA?AViterator@0@AEAV?$CArray@F$0DC@@@@Z'
```

This returns:

```text
Microsoft (R) C++ Name Undecorator
Copyright (C) Microsoft Corporation. All rights reserved.

Undecoration of :-
"??$append@F$0DC@@?$CArray@F$0BO@@@QEAA?AViterator@0@AEAV?$CArray@F$0DC@@@@Z"

is :-
"public: class CArray<short,30>::iterator __cdecl CArray<short,30>::append<short,50>(class CArray<short,50> & __ptr64) __ptr64"
```

The demangled name shows that the uncovered template instantiation corresponds to:

```cpp
CArray<short, 30>::append<short, 50>(
    CArray<short, 50>&
);
```

This makes it possible to construct a unit test that exercises the exact template instantiation reported as uncovered.

> **PowerShell note:** Use single quotes (`'...'`) around the mangled name. With double quotes (`"..."`), PowerShell interprets `$...` sequences as variables and corrupts the mangled symbol.
