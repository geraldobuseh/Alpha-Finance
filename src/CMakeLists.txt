# Personal Quant Lab - Library and Executable Configuration

add_library(pql_domain STATIC
    domain/financial_types.cpp
    domain/market_data.cpp
    domain/order.cpp
    domain/trade.cpp
    domain/position.cpp
    domain/portfolio.cpp
    domain/transaction.cpp
    analytics/daily_valuation.cpp
)
target_include_directories(pql_domain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(pql_domain PUBLIC cxx_std_20)
add_library(pql_execution STATIC execution/fake_broker.cpp)
target_link_libraries(pql_execution PUBLIC pql_domain)
add_library(pql_simulation STATIC strategy/spy_buy_and_hold.cpp strategy/dollar_cost_averaging.cpp
    strategy/momentum.cpp
    simulation/paper_portfolio_pair.cpp)
target_link_libraries(pql_simulation PUBLIC pql_execution)
add_library(pql_market_data STATIC market_data/fake_market_data_provider.cpp)
target_link_libraries(pql_market_data PUBLIC pql_domain)
target_compile_features(pql_market_data PUBLIC cxx_std_20)
if(PQL_BUILD_PERSISTENCE)
    add_subdirectory(persistence)
endif()
if(PQL_BUILD_REAL_MARKET_DATA)
    add_library(pql_alpha_vantage STATIC market_data/alpha_vantage.cpp market_data/curl_http_transport.cpp)
    target_link_libraries(pql_alpha_vantage PUBLIC pql_domain PRIVATE CURL::libcurl nlohmann_json::nlohmann_json)
    if(PQL_BUILD_PERSISTENCE)
        add_executable(pql_ingest_daily ingest_daily.cpp)
        target_link_libraries(pql_ingest_daily PRIVATE pql_alpha_vantage pql_persistence nlohmann_json::nlohmann_json)
    endif()
endif()

# Create the main executable
add_executable(pql_app main.cpp)
target_link_libraries(pql_app PRIVATE pql_domain)

# Set target properties
target_compile_features(pql_app PRIVATE cxx_std_20)

# Install target (optional)
install(TARGETS pql_app DESTINATION bin)
