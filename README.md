# 4C

4C ("Comprehensive Computational Community Code") is a parallel multiphysics research code
to address a plethora of physical problems by means of _computational mechanics_.

Large parts of 4C are based on finite element methods (FEM),
but alternative discretization methods such as discontinuous Galerkin methods (DG),
particle methods and mesh-free methods have also been successfully integrated.
The research software is implemented throughout in object-oriented programming (C++)
using modern software design and is parallelized with MPI for distributed memory hardware architectures.

**Disclaimer**: 4C is developed for research purposes in the field of numerical method development.
It is not intended for any use beyond this purpose, and generally should not be used for any form of
safety-relevant or safety-critical calculations,
or for an application in association with physical products in particular.

## Contents

1. [Getting Up and Running with 4C](#getting-up-and-running-with-4c)
    1. [Set-up LRZ GitLab account](#set-up-lrz-gitlab-account)
    1. [Configure Git](#configure-git)
    1. [Clone the Repository](#clone-the-repository)
    1. [Set-Up the Environment](#set-up-the-environment)
    1. [Configure and Build](#configure-and-build)
    1. [Prepare and Run Simulations](#prepare-and-run-simulations)
    1. [Updating 4C](#updating-4c)
1. [Where to Ask Questions](#where-to-ask-questions)
1. [Contributing](#contributing)
1. [How to cite 4C](#how-to-cite-4c)
1. [License](#license)

## Getting Up and Running with 4C

### Set-up LRZ GitLab account

1. Register an account on [LRZ GitLab](https://gitlab.lrz.de/).

   **Important:** Choose a recognizable user name. It is recommended to set it to: first letter of first name followed
   by last name, all lowercase, e.g., Max Mustermann -> mmustermann.

   > **Note:** Your username is a unique namespace related to your user ID. Changing it can have unintended side
   effects.
   See [how redirects will behave](https://gitlab.lrz.de/help/user/project/index.md#redirects-when-changing-repository-paths)
   for details.

1. Go to your GitLab profile settings and update your profile settings, in particular your
    * First and last name
    * Institute Email Address
1. Select proper notification settings. We recommend *Watching* or *On mention* to guarantee that you don't miss any
   important developments and discussions.
1. Add your public SSH key found in `~/.ssh/id_rsa.pub` to your user profile.

Our user documentation
provides [detailed setup instructions for your GitLab account](https://baci.pages.gitlab.lrz.de/baci/readthedocs/4Csetup.html).

[↑ Contents](#contents)

### Clone the Repository

```bash
cd <someBaseDir>
mkdir <sourceDir>
git clone git@gitlab.lrz.de:baci/baci.git <sourceDir>
cd <sourceDir>
```

where `<someBaseDir>` is some directory on your machine and `<sourceDir>` will contain the 4C source code.
You can choose names and locations of these directories freely.

Your directory tree should look like the following:

```
<someBaseDir>/
  <sourceDir>
```

[↑ Contents](#contents)

### Configure Git

A Git version >= 2.9 is
required. <!-- We need at least this version to be able to configure the path to the git-hooks as outlined below. -->
Consult the official [Git documentation](www.git-scm.org) to obtain a more recent Git installation if necessary.

1. Set your username to your full name, i.e., first name followed by last name,
   and your email address to your institute email address with the following commands:

    ```bash
    git config --global user.name "<Firstname> <Lastname>"
    git config --global user.email <instituteEmailAddress>
    ```

1. Set a default text editor that will be used whenever you need to write a message in Git. To set `kwrite` as your
   default text editor, type:

    ```bash
    git config --global core.editor kwrite
    ```

   > **Note:** Another popular choice is `vim`.

Our user documentation provides
a [detailed setup guide for your local git configuration](https://baci.pages.gitlab.lrz.de/baci/readthedocs/4Csetup.html#set-up-your-gitlab-account).

[↑ Contents](#contents)

### Set-up the Environment

4C heavily relies on the [Trilinos project](https://trilinos.github.io).

Some further third party libraries (TPLs) are mandatory, e.g.

- ParMETIS (recommended version: 4.0.3)
- SuiteSparse (recommended version: 5.4.0)
- SuperLUDist (recommended version: 7.2)
- Qhull (recommended version: 2012.1)
- CLN (recommened version: 1.3.4)

and some are optional, e.g.

- FFTW
- [ArborX](https://github.com/arborx/ArborX)
- [MIRCO](https://github.com/imcs-compsim/MIRCO/)

Often, a pre-compiled version of Trilinos and set of TPLs is available at your institute.
Look into the CMake presets in `presets/` or ask your colleagues for further information.

Some helper scripts to install these TPLs can be found in `dependencies/`.

Additional information can be found in
the [user documentation](https://baci.pages.gitlab.lrz.de/baci/readthedocs/4Csetup.html#external-dependencies).

[↑ Contents](#contents)

### Configure and Build

#### Create python virtual environment for 4C development (optional)

For testing and active development, you need to create a python virtual environment once. In the source directory,
execute:

```
./set_up_dev_env.sh
```

#### Create the Build Directory

4C enforces an out-of-source build, i.e. your build directory may not be located inside the source code directory.

```bash
cd <someBaseDir>
mkdir <buildDir>
cd <buildDir>
```

where `<buildDir>` is your build directory.

#### Configure

Run

```bash
cmake --preset=<name-of-preset> ../<sourceDir> | tee config$(date +%y%m%d%H%M%N).log
```

> **Note:**  When you see `command |& tee something$(date +%y%m%d%H%M%N).log`, that is just a means of running a command
> and sending the output both to the screen and to a timestamped log file. This is by no means necessary, but if you run
> into problems, having these timestamped log files can be quite useful in debugging what's gone wrong.

A preset name needs to be passed to cmake via the command line argument `--preset`, as indicated above.
Use `cmake ../<sourceDir> --list-presets` to get a list of all available presets.

More information about the cmake presets can be found in the
[user documentation](https://baci.pages.gitlab.lrz.de/baci/readthedocs/4Csetup.html#cmakepresets).

**Note:** Make sure to use at least CMake 3.25. Install it in your path or use the ones provided on your institute's
server.

#### Build

```bash
ninja -j <numProcs> full |& tee build$(date +%y%m%d%H%M%N).log
```

where `<numProcs>` is the number of processors you want to use.

> **Note:**  After the first build, it is rarely necessary to reconfigure 4C &mdash; only the build-command is
> required. `cmake` is invoked *automatically* during the build process if something changed within `CMakeLists.txt`.

> **Note:** Make sure to have Ninja installed on your system.

#### Run the Tests

To verify that the build was successful, run the minimal set of tests via

```bash
ctest -L minimal
```

or all tests via

```bash
ctest
```

You can use the option `-j <num_threads>` to specify the number of threads to be used for parallel
execution.

[↑ Contents](#contents)

### Prepare and Run Simulations

After sucessfully building 4C, the executable `4C` is located in your build directory `<buildDir>/`.
It needs to be invoked together with an input (`.dat`) file via

```bash
<buildDir>/4C <jobName>.dat <outputName>
```

where `<jobName>` is the name of the simulation file and `<outputName>` denotes the name of the corresponding output
file(s).
A collection of working `.dat` files is located under `<sourceDir>/tests/input_files/`.

In case you used the binary output option, your simulation results can be accessed using the `post_processor`
script which is located in the build directory.
Run

```bash
<buildDir>/post_processor --file=<outputName> [options]
```

on the result (`.control`) file of your simulation to make the accessible for visualization tools like `Paraview`.
Type

```bash
<buildDir>/post_processor --help
```

to list all options that are available to filter the output.

If you are using runtime ouput, the simulation results are directly accessible in vtk format.

The input (`.dat`) file can be created using the `pre_exodus` script which is also located in the 4C build directory.
The script is invoked via

```bash
<buildDir>/pre_exodus --exo=<problem>.e --head=<problem>.head --bc=<problem>.bc --dat=<inputFileName>.dat
```

where `<problem>.e` is an exodus file containing the mesh information of your problem,
the `<problem>.bc` file contains the boundary conditions and element information and
the `<problem>.head` file contains the remaining simulation settings.
Prototypes of the `.head` and `.bc` files can be obtained by omitting the `--head=...` and `--bc=...` options in
the `pre_exodus` call.
These prototype files (`default.head` and `default.bc`) contain all currently implemented conditions, element types,
material models,
simulation settings etc....

[↑ Contents](#contents)

### Updating 4C

Any time you need to grab the latest from 4C:

```bash
cd <someBaseDir>/<sourceDir>
git checkout main
git pull
```

[↑ Contents](#contents)

## Where to Ask Questions

If you need help with 4C, feel free to ask questions
by [creating a GitLab issue](https://gitlab.lrz.de/baci/baci/issues). Use an issue template from the dropdown menu to
pre-populate the *Description* field, giving you instructions on submitting the issue.

[↑ Contents](#contents)

## Contributing

If you're interested in contributing to 4C, we welcome your collaboration. Before your start, configure
your [local Git](#set-up-git) and your [LRZ GitLab account](#set-up-lrz-gitlab).
Read [our contributing guidelines](https://gitlab.lrz.de/baci/baci/blob/main/CONTRIBUTING.md) carefully for details on
our workflow, submitting merge-requests, etc.

[↑ Contents](#contents)

## How to cite 4C

Whenever you mention 4C in some sort of scientific document/publication/presentation, please cite 4C as follows:

```
4C: A Comprehensive Multi-Physics Simulation Framework, https://www.4c-multiphysics.org
```

You could use the following BibTeX entry:

```bibtex
@misc{4C,
  author       = {{4C}},
  title        = {{4C}: A {C}omprehensive {M}ulti-{P}hysics {S}imulation {F}ramework},
  howpublished = {\url{https://www.4c-multiphysics.org}},
  year         = {YEAR},
  note         = {Accessed: DATE}
}
```

_Note:_ You might need to adapt the BibTeX entry depending on the citation style.

Remember: It is good scientific practice to include the date, when you've visisted that website, into the citation. It's
up to you (and your advisor) to include the date, depending on the type of publication.

[↑ Contents](#contents)

## License

ADD A LICENSE!!

[↑ Contents](#contents)
