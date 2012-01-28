LOCAL_PATH:= $(call my-dir)

#
# hidd
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	hidd.c sdp.c fakehid.c

LOCAL_CFLAGS:= \
	-DVERSION=\"4.47\" \
	-DSTORAGEDIR=\"/data/misc/bluetoothd\" \
	-DNEED_PPOLL

LOCAL_C_INCLUDES:=\
	$(LOCAL_PATH)/../include \
	$(LOCAL_PATH)/../common \

LOCAL_SHARED_LIBRARIES := \
	libbluetooth

LOCAL_STATIC_LIBRARIES := \
	libbluez-common-static

LOCAL_MODULE:=hidd

include $(BUILD_EXECUTABLE)

#
# pand
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	pand.c bnep.c sdp.c

LOCAL_CFLAGS:= \
	-DVERSION=\"4.47\" \
	-DSTORAGEDIR=\"/data/misc/bluetoothd\" \
	-DNEED_PPOLL

LOCAL_C_INCLUDES:=\
	$(LOCAL_PATH)/../include \
	$(LOCAL_PATH)/../common \

LOCAL_SHARED_LIBRARIES := \
	libbluetooth

LOCAL_STATIC_LIBRARIES := \
	libbluez-common-static

LOCAL_MODULE:=pand

include $(BUILD_EXECUTABLE)

#
# dund
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	dund.c sdp.c dun.c msdun.c

LOCAL_CFLAGS:= \
	-DVERSION=\"4.47\" \
	-DSTORAGEDIR=\"/data/misc/bluetoothd\" \
	-DNEED_PPOLL

LOCAL_C_INCLUDES:=\
	$(LOCAL_PATH)/../include \
	$(LOCAL_PATH)/../common \

LOCAL_SHARED_LIBRARIES := \
	libbluetooth

LOCAL_STATIC_LIBRARIES := \
	libbluez-common-static

LOCAL_MODULE:=dund

include $(BUILD_EXECUTABLE)
