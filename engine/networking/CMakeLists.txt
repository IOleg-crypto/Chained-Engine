set(NETWORK_SOURCES
    network_types.h
    network_driver.h
    enet_threaded_driver.h
    enet_threaded_driver.cpp
    network_session.h
    network_session.cpp
    network_transport.h
    network_transport.cpp
    network_player_manager.h
    network_player_manager.cpp
    network_service.h
    network_service.cpp
    net_packet.h
    stun_client.h
    stun_client.cpp
    upnp_port_mapper.h
    upnp_port_mapper.cpp
)

add_library(engine_network STATIC
    ${NETWORK_SOURCES}
)

target_include_directories(engine_network PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(engine_network PUBLIC
    engine_core
    engine_project
    engine_external_entt
    enet::enet_static
    glm
    engine_external_miniupnpc
)

if(WIN32)
    target_link_libraries(engine_network PUBLIC ws2_32)
endif()

if(TARGET engine_pch)
    target_link_libraries(engine_network PRIVATE engine_pch)
endif()
