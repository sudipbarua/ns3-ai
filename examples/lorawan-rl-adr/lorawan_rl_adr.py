import copy
from typing import List
import numpy as np
import ns3ai_ratecontrol_ts_py as py_binding
from ns3ai_utils import Experiment
import sys
import traceback




# Some simple configuration parameters to be passed to ns3
ns3Settings = {
    'verbose': False,
    'adrEnabled': True,
    'nDevices': 1,
    'nGateways': 1,
    'nPeriodsOf20Minutes': 1,
    'sideLengthMeters': 1000,
    'maxRandomLossDB': 10,
    'adrType': 'LorawanRlAdr',
    }

exp = Experiment("ns3ai_lorawan_rl_adr", "../../../../../", py_binding, handleFinish=True)
msgInterface = exp.run(setting=ns3Settings, show_output=True)
random_stream = 100
c = AiAdrContainer(msgInterface=msgInterface, stream=random_stream)

try:
    while True:
        c.msgInterface.PyRecvBegin()
        c.msgInterface.PySendBegin()
        if c.msgInterface.PyGetFinished():
            break
        c.do(c.msgInterface.GetCpp2PyStruct(), c.msgInterface.GetPy2CppStruct())
        c.msgInterface.PyRecvEnd()
        c.msgInterface.PySendEnd()

except Exception as e:
    exc_type, exc_value, exc_traceback = sys.exc_info()
    print("Exception occurred: {}".format(e))
    print("Traceback:")
    traceback.print_tb(exc_traceback)
    exit(1)

else:
    pass

finally:
    print("Finally exiting...")
    del exp
