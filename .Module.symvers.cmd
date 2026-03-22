cmd_/home/deep/counterdev/Module.symvers := sed 's/\.ko$$/\.o/' /home/deep/counterdev/modules.order | scripts/mod/modpost -m -a  -o /home/deep/counterdev/Module.symvers -e -i Module.symvers   -T -
