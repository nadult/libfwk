import sys
import os

if sys.version_info[0] < 3:
    import imp
    cur_dir = os.path.dirname(os.path.realpath(__file__))
    fwk_ycm = imp.load_source('module.name', cur_dir + '/../../.ycm_extra_conf.py')
else:
    raise Exception("Write me")

import os
import ycm_core

llvm_path = "/usr/lib/llvm-8/include"
fwk_ycm.flags = fwk_ycm.flags + ['-isystem', llvm_path ]

SOURCE_EXTENSIONS = fwk_ycm.SOURCE_EXTENSIONS
DirectoryOfThisScript = fwk_ycm.DirectoryOfThisScript
MakeRelativePathsInFlagsAbsolute = fwk_ycm.MakeRelativePathsInFlagsAbsolute
IsHeaderFile = fwk_ycm.IsHeaderFile
GetCompilationInfoForFile = fwk_ycm.GetCompilationInfoForFile
FlagsForFile = fwk_ycm.FlagsForFile
