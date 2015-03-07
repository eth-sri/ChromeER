### Getting the source and building Chromium/ER

1. Install `depot_tools` as described here:

    http://dev.chromium.org/developers/how-tos/install-depot-tools

1. Create a Chromium working directory, e.g.

    mkdir ~/chromium

1. Clone this GitHub repository with the following command:

    cd ~/chromium; git clone https://github.com/eth-srl/ChromeER.git src

    Note that the repository is cloned in the `src` subdirectory. By default this command will checkout the `srl` branch.

1. Create a file named `.gclient` in the `chromium` directory (above `src`), with the following content:<br>
   solutions = [{ <br>
        u'managed': False,  <br>
        u'name': u'src', <br>
        u'url': u'https://github.com/eth-srl/ChromeER.git',  <br>
        u'custom_deps': {},  <br>
        u'deps_file': u'.DEPS.git',  <br>
    }]<br>
    target_os = ['all']

1. (GNU/Linux only) From the `src` directory run:
   
    build/install-build-deps.sh

    If the above does not work directly (sometimes it asks to install mingw packages on Linux), then an alternative command is:
    
    build/install-build-deps.sh --no-nacl

1. From the `src` directory run: 
    
    gclient sync

1. Checkout the srl branch for V8 and BlinkER to get the latest changes

    cd ~/chromium/src/v8; git checkout srl
    
    cd ~/chromium/src/third_party/WebKit; git checkout srl

1. Build Chromium with:
    
    cd ~/chromium/src/out/Debug; ninja chrome

1. To run without sandbox on, type:

    ./chrome --no-sandbox

    Otherwise, to build with sandbox, follow: 

    https://code.google.com/p/chromium/wiki/LinuxSUIDSandboxDevelopment

1. For more information, consult the developer pages at http://chromium.org

### Some notes about debugging on GNU/Linux

1. On some systems (e.g. Ubuntu) it may be necessary to give the user the necessaty `prtace` permissions. Do this with the command:

    echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope

1. On the `srl` branch, the build is modified not to tell `gold` to create `.gdb_index` section. Instead use `build/gdb-add-index`. Thus we ensure the `gdb` can understand and use the `.gdb_index` section format.


### Misc notes

1. The SRL modifications are based on the `lkgr` branch, which is occasionally merged into the `srl` branch. The tag `srl-base` is updated to point to the last merged revision on the `lkgr` branch, thus `git diff srl-base srl` will show the EventRacer related changes to the original sources.
