# CCAnnotator
load a code coverage project and annotates the html pages based on comments in the source code

for the ctag use something like call e.g. 
ctags --fields=+ne --c-kinds=f --output-format=json -f ./tag AcqData.h AcqData.cpp

The process is automated by the command line script create_tags.cmd. You just have to specify the required files in the script.
