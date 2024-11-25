#! /bin/bash

export PATH=/opt/st/stm32cubeide_1.13.2/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.11.3.rel1.linux64_1.1.1.202309131626/tools/bin:/opt/st/stm32cubeide_1.13.2/plugins/com.st.stm32cube.ide.mcu.externaltools.make.linux64_2.1.0.202305091550/tools/bin:/home/ayeiser/.cargo/bin:/usr/local/bin:/usr/local/sbin:/usr/bin:/usr/sbin:/home/ayeiser/.local/bin

cd Release

compiledb -o ../compile_commands.json make -j16 all
