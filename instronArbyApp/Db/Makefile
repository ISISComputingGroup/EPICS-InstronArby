TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

DB += controls.db 
DB += controls_channel.db 
DB += controls_channel_specific.db
DB += controls_waveform.db
DB += logging.db

#----------------------------------------------------
# Create and install (or just install) into <top>/db
# databases, templates, substitutions like this
DATA += devinstron.proto

#----------------------------------------------------
# If <anyname>.db template is not named <anyname>*.template add
# <anyname>_template = <templatename>

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

