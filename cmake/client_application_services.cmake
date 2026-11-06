# Retain the existing class namespace's System-output service for worker users.
target_sources(mc_client_application_owner PRIVATE src/client/startup/application_services.c)
