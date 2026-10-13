# CCAnnotator
load a code coverage project and annotates the html pages based on comments in the source code

for the ctag use something like call e.g. 
ctags --fields=+ne --c-kinds=f --output-format=json -f ./tag AcqData.h AcqData.cpp

The process is automated by the command line script create_tags.cmd. You just have to specify the required files in the script.

## Loading the project and subprojects
After cloning without submodules, use 
To update the submodules use:
git submodule update --init --recursive
The submodules may be stripped off in future releases as they are bypassed in the current implementation.

## Activate profiling with clang
Activating code coverage with clang-cl could be tricky. 
Specify the flags
-fprofile-instr-generate -fcoverage-mapping 
in the compiler settings.

## Create profiling files
### Step one
Call
$env:LLVM_PROFILE_FILE="coverage.profraw"; & ".\x64\Unit Test - Release\P300_SignalProcessing.exe"
from the root project directory
### Second step
Create new data file
lvm-profdata merge -sparse coverage.profraw -o coverage.profdata

in powershell, this look like this, e.g.:
$llvm = "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\Llvm\x64\bin"
& "$llvm\llvm-profdata.exe" merge -sparse coverage.profraw -o coverage.profdata

### Third step
Create/show a record of all executed tests:
llvm-cov report "./x64/Unit Test - Release/P300_SignalProcessing.exe" -instr-profile="coverage.profdata"

Filename                                       Regions    Missed Regions     Cover   Functions  Missed Functions  Executed       Lines      Missed Lines     Cover    Branches   Missed Branches     Cover

Create report:
llvm-cov show ".\P300_SignalProcessing.exe" -instr-profile=".\coverage.profdata" -format=html -output-dir="coverage_ProcessData" "ProcessData.cpp"

or for multiple file:

llvm-cov show ".\P300_SignalProcessing.exe"
    -instr-profile=".\coverage.profdata"
    -format=html
    -output-dir="coverage_report"
    "ProcessData.cpp"
    "OtherFile.cpp"

e.g.
llvm-cov show ".\P300_SignalProcessing.exe"
    -instr-profile=".\coverage.profdata"
    -format=html
    -output-dir="coverage_report"
    "carray.h"
    "dsparray.h"
    "ProcessData.cpp"
