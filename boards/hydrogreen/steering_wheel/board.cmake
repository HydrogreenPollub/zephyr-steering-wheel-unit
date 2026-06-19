# SPDX-License-Identifier: Apache-2.0

board_runner_args(jlink "--device=STM32G491CE" "--speed=4000"
		  "--gdbserver=C:/Program Files/SEGGER/JLink/JLinkGDBServer.exe")
board_runner_args(stm32cubeprogrammer "--port=swd" "--reset-mode=hw")

include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
include(${ZEPHYR_BASE}/boards/common/openocd.board.cmake)
include(${ZEPHYR_BASE}/boards/common/stm32cubeprogrammer.board.cmake)
