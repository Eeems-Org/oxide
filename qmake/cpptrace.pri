linux-oe-g++{
    CONFIG(debug, debug|release){
        CPPTRACE_ROOT = $$OUT_PWD/../../shared/cpptrace
        CPPTRACE_LIB = $$CPPTRACE_ROOT/lib
        CPPTRACE_INC = $$CPPTRACE_ROOT/include
        LIBS += -L$$CPPTRACE_LIB -lcpptrace -ldwarf -lz -ldl
        INCLUDEPATH += $$CPPTRACE_INC
        DEPENDPATH += $$CPPTRACE_LIB
        QMAKE_LFLAGS += -rdynamic
    }
}
