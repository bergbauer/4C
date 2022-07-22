#! ./utilities/baci-python-venv/bin/python3

import os
import sys
import subprocess

try:
    from lxml import etree
except ImportError:
    print("\n Error: python-lxml is not installed. For installation type as root:")
    print("   yum install python-lxml ")
    print(" exiting now...\n")
    sys.exit(1)

import random
import string
from utils import get_mpi_include_path


def getCompilerPaths():
    """Get compiler paths using 'g++ -v -E -P -dD'"""
    comppath = set()

    # to avoid filename clashes
    save = "".join([random.choice(string.ascii_letters) for x in range(30)])
    complicated_name_cpp = "dummy_" + save + ".cpp"

    # write path information into output file
    with open(complicated_name_cpp, "w") as f:
        f.write("\n")
    output = subprocess.check_output(
        "g++ -v -E -P -dD " + complicated_name_cpp,
        encoding="UTF-8",
        shell=True,
        stderr=subprocess.STDOUT,
    )
    import time

    time.sleep(1)
    os.system("rm " + complicated_name_cpp)

    # parse output and store compiler paths in set
    paths_are_comming = False
    for l in output.splitlines():
        if l.find("#include <...> search starts here:") == 0:
            paths_are_comming = True
        else:
            if l.find("End of search list") == 0:
                paths_are_comming = False
            if paths_are_comming:
                comppath.add(l.strip())

    return comppath


def getPaths(build_folder):
    """Get all include paths"""
    pathlist = set()

    # get paths specified in the do-configure file from the cmake cache
    cachefile = os.path.join(build_folder, "CMakeCache.txt")
    with open(cachefile, "r") as f:
        for l in f.readlines():
            if l.find("INCLUDE_INSTALL_DIR:PATH=") > -1:
                pathlist.add(l.split("=")[1][0:-1])
            if l.find("Trilinos_DIR:PATH=") > -1:
                pathlist.add(l.split("=")[1][0:-1] + "/../../../include")

    # add the cmake generated header directory
    pathlist.add(os.path.join(build_folder, "src", "headers"))

    # Get the openmpi include directory.
    open_mpi_dir = get_mpi_include_path()
    if open_mpi_dir is not None:
        pathlist.add(open_mpi_dir)

    # add compiler paths
    pathlist.update(getCompilerPaths())

    return pathlist


def getDefineValue(build_folder):
    """Get all define flags that specify a value from the CMakeFiles folder"""
    definevalueset = set()
    symbolset = set()

    with open(build_folder + "/CMakeFiles/drt_lib.dir/flags.make", "r") as f:
        for l in f.readlines():
            if l.startswith("CXX_FLAGS"):
                if "-std=c++17" in l:
                    definevalueset.add(("__cplusplus", "201703L"))
            elif l.startswith("CXX_DEFINES"):
                for w in l.split():
                    if w.startswith("-D"):
                        flag = w[2:]
                        if "=" in flag:
                            defineflag, val = flag.split("=", 1)
                            definevalueset.add((defineflag, val))
                        else:
                            symbolset.add(flag)

    definevaluelist = []
    symbollist = []

    definevaluelist = [x for x in definevalueset]
    definevaluelist.sort()

    symbollist = [x for x in symbolset]
    symbollist.sort()

    return (definevaluelist, symbollist)


def adapt(do_configure_file, build_folder, build_type):
    """update .cproject file if existing"""

    print(build_folder)

    if os.path.isfile(".cproject"):  # if file exists
        project = etree.parse(".cproject")

        pathset = getPaths(build_folder)
        pathlist = [x for x in pathset]
        pathlist.sort()

        definevaluelist, symbollist = getDefineValue(build_folder)

        # iterate over all entries named 'option'
        found_path = False
        found_symbol = False
        for option in project.iter("option"):
            superClass = option.get("superClass")

            if superClass.endswith(
                "org.eclipse.cdt.build.core.settings.holder.incpaths"
            ) or superClass.endswith("compiler.option.include.paths"):
                # print(etree.tostring(option, pretty_print=True))
                for entry in option:
                    option.remove(entry)
                for entry in pathlist:
                    option.append(
                        etree.Element("listOptionValue", builtIn="false", value=entry)
                    )
                # print(etree.tostring(option, pretty_print=True))
                found_path = True

            if (
                superClass.endswith("gnu.cpp.compiler.option.preprocessor.def")
                or superClass.endswith("gnu.c.compiler.option.preprocessor.def.symbols")
                or superClass.endswith(
                    "org.eclipse.cdt.build.core.settings.holder.symbols"
                )
            ):
                # print(etree.tostring(option, pretty_print=True))
                for entry in option:
                    option.remove(entry)
                for entry in symbollist:
                    option.append(
                        etree.Element("listOptionValue", builtIn="false", value=entry)
                    )
                for entry in definevaluelist:
                    option.append(
                        etree.Element(
                            "listOptionValue",
                            builtIn="false",
                            value="{}={}".format(entry[0], entry[1]),
                        )
                    )
                # print(etree.tostring(option, pretty_print=True))
                found_symbol = True

        if not found_path:
            print(
                "Please add manually (Eclipse) any path to the project's include path section to create an initial entry in '.cproject'"
            )
        if not found_symbol:
            print(
                "Please add manually (Eclipse) any symbol to the project's symbol section to create an initial entry in '.cproject'"
            )

        # print(etree.tostring(root, pretty_print=True))
        fo = open(".cproject", "w")
        fo.write('<?xml version="1.0" encoding="UTF-8" standalone="no"?>')
        fo.write("<?fileVersion 4.0.0?>")
        fo.write("")
        fo.write(etree.tostring(project, pretty_print=True).decode("utf-8"))
        fo.close()

        print("++ Update of .cproject file done")


if __name__ == "__main__":
    adapt(sys.argv[1], sys.argv[2], sys.argv[3])
