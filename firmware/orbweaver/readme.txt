How to set-up new CCS project:

1) 	Install TI SimpleLink
2) 	In CCS create a new CCS project.
3) 	In cc13x0f128.cmd, change "ResetISR" to "resetISR"
4) 	Project Properties->Build->Arm Compiler->Include Options->Add dir to #include search path: "${workspace_loc:/${ProjName}/source}"
5) 	Project Properties->Build->Arm Compiler->Include Options->Add dir to #include search path: "C:\ti\simplelink_cc13x0_sdk_4_20_02_07\source"
6) 	Project Properties->Build->Arm Compiler->Include Options->Add dir to #include search path: "C:/ti/simplelink_cc13x0_sdk_4_20_02_07/source/ti/devices/cc13x0"
7) 	Project Properties->Build->Arm Compiler->Predefined Symbols->Pre-define NAME: "DeviceFamily_CC13X0"
8) 	Project Properties->Build->Arm Linker->File Search Path->Include library file or command file as input: "C:\ti\simplelink_cc13x0_sdk_4_20_02_07\source\ti\devices\cc13x0\driverlib\bin\ccs\driverlib.lib"
9) 	Project Properties->Build->Steps->Pre-build steps: python "${PROJECT_ROOT}/update_version.py"
10)	Project Properties->Build->Steps->Post-build steps: python "${PROJECT_ROOT}/generate_bin.py" "${ConfigName}"