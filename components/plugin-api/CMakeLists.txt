# plugin-api is only used when launcher plugins are enabled. On targets where
# plugin support isn't built (ENABLE_LAUNCHERPLUGINS unset, e.g. non-P4
# builds), register as an empty component so the auto-discovery in
# components/ doesn't try to resolve badge-elf-api, which is rule-gated out.
if(CONFIG_ENABLE_LAUNCHERPLUGINS)
    idf_component_register(
        INCLUDE_DIRS "include"
        REQUIRES badge-elf-api
    )
else()
    idf_component_register()
endif()
