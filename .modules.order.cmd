cmd_/home/deep/counterdev/modules.order := {   echo /home/deep/counterdev/counterdev.ko; :; } | awk '!x[$$0]++' - > /home/deep/counterdev/modules.order
