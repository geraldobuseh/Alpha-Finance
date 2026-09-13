# Personal Quant Lab - Test Configuration

# Fetch GoogleTest from GitHub
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP true
)

# For Windows: prevent overriding parent project compiler options
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

find_package(GTest 1.14 CONFIG QUIET)
if(GTest_FOUND)
    add_library(gtest_main ALIAS GTest::gtest_main)
else()
    FetchContent_MakeAvailable(googletest)
endif()

# Create test executable
add_executable(pql_tests test_smoke.cpp)

# Link against GoogleTest
target_link_libraries(pql_tests PUBLIC gtest_main)

# Set C++ standard
target_compile_features(pql_tests PRIVATE cxx_std_20)

# Suppress GoogleTest's overly-aggressive warnings (external dependency)
if(MSVC AND NOT GTest_FOUND)
    target_compile_options(gtest PUBLIC /wd4251)  # class needs dll-interface
    target_compile_options(gtest_main PUBLIC /wd4251)
endif()

# Register test with CTest
add_test(NAME SmokeTest COMMAND pql_tests)

add_executable(pql_domain_tests test_financial_types.cpp)
target_link_libraries(pql_domain_tests PRIVATE pql_domain gtest_main)
add_test(NAME FinancialTypesTest COMMAND pql_domain_tests)

add_executable(pql_order_trade_tests test_order_trade.cpp)
target_link_libraries(pql_order_trade_tests PRIVATE pql_domain gtest_main)
add_test(NAME OrderTradeTest COMMAND pql_order_trade_tests)

add_executable(pql_position_tests test_position.cpp)
target_link_libraries(pql_position_tests PRIVATE pql_domain gtest_main)
add_test(NAME PositionTest COMMAND pql_position_tests)

add_executable(pql_portfolio_tests test_portfolio.cpp)
target_link_libraries(pql_portfolio_tests PRIVATE pql_domain gtest_main)
add_test(NAME PortfolioTest COMMAND pql_portfolio_tests)

add_executable(pql_transaction_tests test_transaction.cpp)
target_link_libraries(pql_transaction_tests PRIVATE pql_domain gtest_main)
add_test(NAME TransactionTest COMMAND pql_transaction_tests)

add_executable(pql_market_data_tests test_market_data.cpp)
target_link_libraries(pql_market_data_tests PRIVATE pql_domain gtest_main)
add_test(NAME MarketDataTest COMMAND pql_market_data_tests)

add_executable(pql_fake_market_data_tests test_fake_market_data.cpp)
target_link_libraries(pql_fake_market_data_tests PRIVATE pql_market_data gtest_main)
add_test(NAME FakeMarketDataTest COMMAND pql_fake_market_data_tests)

# Interface contract is checked at compile time; no concrete broker or I/O.
add_executable(pql_broker_tests test_broker.cpp)
target_link_libraries(pql_broker_tests PRIVATE pql_domain)
add_test(NAME BrokerContractTest COMMAND pql_broker_tests)

add_executable(pql_fake_broker_tests test_fake_broker.cpp)
target_link_libraries(pql_fake_broker_tests PRIVATE pql_execution gtest_main)
add_test(NAME FakeBrokerTest COMMAND pql_fake_broker_tests)

add_executable(pql_strategy_tests test_strategy.cpp)
target_link_libraries(pql_strategy_tests PRIVATE pql_execution gtest_main)
add_test(NAME StrategyTest COMMAND pql_strategy_tests)

add_executable(pql_spy_benchmark_tests test_spy_benchmark.cpp)
target_link_libraries(pql_spy_benchmark_tests PRIVATE pql_simulation gtest_main)
add_test(NAME SpyBenchmarkTest COMMAND pql_spy_benchmark_tests)

add_executable(pql_dca_tests test_dollar_cost_averaging.cpp)
target_link_libraries(pql_dca_tests PRIVATE pql_simulation gtest_main)
add_test(NAME DollarCostAveragingTest COMMAND pql_dca_tests)

if(PQL_BUILD_POSTGRES_TESTS)
    add_executable(pql_postgres_tests test_postgres.cpp)
    target_link_libraries(pql_postgres_tests PRIVATE pql_persistence gtest_main libpqxx::pqxx)
    add_test(NAME PostgresIntegrationTest COMMAND pql_postgres_tests)
    set_tests_properties(PostgresIntegrationTest PROPERTIES LABELS integration TIMEOUT 60)
endif()
if(PQL_BUILD_REAL_MARKET_DATA)
    add_executable(pql_alpha_vantage_tests test_alpha_vantage.cpp)
    target_link_libraries(pql_alpha_vantage_tests PRIVATE pql_alpha_vantage gtest_main nlohmann_json::nlohmann_json)
    add_test(NAME AlphaVantageTest COMMAND pql_alpha_vantage_tests)
    if(PQL_BUILD_POSTGRES_TESTS)
        add_executable(pql_daily_ingestion_tests test_daily_ingestion.cpp)
        target_link_libraries(pql_daily_ingestion_tests PRIVATE pql_alpha_vantage pql_persistence gtest_main libpqxx::pqxx nlohmann_json::nlohmann_json)
        add_test(NAME DailyIngestionTest COMMAND pql_daily_ingestion_tests)
        set_tests_properties(DailyIngestionTest PROPERTIES LABELS integration TIMEOUT 60)
    endif()
endif()
