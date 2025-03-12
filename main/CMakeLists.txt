#idf_component_register(SRCS "remoteLED.c"
#                    INCLUDE_DIRS ".")

idf_component_register(SRCS "remoteLED.c"
                       INCLUDE_DIRS "."
                       EMBED_TXTFILES cert.pem)

