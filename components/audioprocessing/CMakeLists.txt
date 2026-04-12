idf_component_register(SRCS "audioprocessing.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES m5mic mfcc_op fft_op svm_model display_p)
