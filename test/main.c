#include <pthread.h>

#include "app_config.h"
#include "unity.h"
#include "unity_fixture.h"

static void RunAllTests( void )
{
  RUN_TEST_GROUP( JsonParser );
  RUN_TEST_GROUP( httpApi );
}

int main( int argc, const char* argv[] )
{
  return UnityMain( argc, argv, RunAllTests );
}