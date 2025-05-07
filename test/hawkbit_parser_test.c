#include <string.h>

#include "hawkbit_parser.h"
#include "unity.h"

void setUp( void )
{
  // Set up code if needed
}

void tearDown( void )
{
  // Tear down code if needed
}

void test_HAWKBITParser_ParseUrl( void )
{
  const char* jsonString = "{\"_links\":{\"configData\":{\"href\":\"http://example.com/config\"},\"deploymentBase\":{\"href\":\"http://192.168.1.2:8090/default/controller/v1/SN6870/deploymentBase/4?c=414400233\"},\"cancelAction\":{\"href\":\"http://example.com/cancel\"}}}";
  char urlConfigData[HAWKBIT_URL_SIZE];
  char urlDeploymentBase[HAWKBIT_URL_SIZE];
  char urlCancelAction[HAWKBIT_URL_SIZE];

  bool result = HAWKBITParser_ParseUrl( jsonString, urlConfigData, sizeof( urlConfigData ), urlDeploymentBase, sizeof( urlDeploymentBase ), urlCancelAction, sizeof( urlCancelAction ) );

  TEST_ASSERT_TRUE( result );
  TEST_ASSERT_EQUAL_STRING( "http://example.com/config", urlConfigData );
  TEST_ASSERT_EQUAL_STRING( "http://192.168.1.2:8090/default/controller/v1/SN6870/deploymentBase/4?c=414400233", urlDeploymentBase );
  TEST_ASSERT_EQUAL_STRING( "http://example.com/cancel", urlCancelAction );
}

void test_HAWKBITParse_ParseDeployment( void )
{
  const char* jsonString = "{\"deployment\":{\"update\":\"forced\",\"download\":\"soft\",\"chunks\":[{\"part\":\"part1\",\"version\":\"v1\",\"name\":\"chunk1\",\"artifacts\":[{\"filename\":\"file1\",\"size\":123,\"_links\":{\"download-http\":{\"href\":\"http://example.com/file1\"}}}]}]}}";
  hawkbit_deployment_t deployment;

  bool result = HAWKBITParse_ParseDeployment( jsonString, &deployment );

  TEST_ASSERT_TRUE( result );
  TEST_ASSERT_EQUAL( HAWKBIT_DEPLOYMENT_ACTION_FORCED, deployment.update );
  TEST_ASSERT_EQUAL( HAWKBIT_DEPLOYMENT_ACTION_SOFT, deployment.download );
  TEST_ASSERT_EQUAL( 1, deployment.chunkSize );
  TEST_ASSERT_EQUAL_STRING( "part1", deployment.chunk[0].part );
  TEST_ASSERT_EQUAL_STRING( "v1", deployment.chunk[0].version );
  TEST_ASSERT_EQUAL_STRING( "chunk1", deployment.chunk[0].name );
  TEST_ASSERT_EQUAL( 1, deployment.chunk[0].artifactsSize );
  TEST_ASSERT_EQUAL_STRING( "file1", deployment.chunk[0].artifacts[0].filename );
  TEST_ASSERT_EQUAL( 123, deployment.chunk[0].artifacts[0].size );
  TEST_ASSERT_EQUAL_STRING( "http://example.com/file1", deployment.chunk[0].artifacts[0].download_http );
}

void test_HAWKBITParser_ParseDeployment_2( void )
{
  const char* jsonString = "{\"id\":\"4\",\"deployment\":{\"download\":\"forced\",\"update\":\"forced\",\"chunks\":[{\"part\":\"os\",\"version\":\"0.1.01\",\"name\":\"TestOS\",\"artifacts\":[{\"filename\":\"main.bin\",\"hashes\":{\"sha1\":\"555f74c85c421734a962e815f25ba7223c249b36\",\"md5\":\"7a2be117d0576f981a06c309ba193afe\",\"sha256\":\"093a5e9d36e6f8e7e897ed8d731b274a31c8d98ff9b532704f93ef1eba259b48\"},\"size\":1167968,\"_links\":{\"download-http\":{\"href\":\"http://192.168.1.2:8090/DEFAULT/controller/v1/SN6870/softwaremodules/20/artifacts/main.bin\"},\"md5sum-http\":{\"href\":\"http://192.168.1.2:8090/DEFAULT/controller/v1/SN6870/softwaremodules/20/artifacts/main.bin.MD5SUM\"}}}]}]}}";
  hawkbit_deployment_t deployment;

  bool result = HAWKBITParse_ParseDeployment( jsonString, &deployment );

  TEST_ASSERT_TRUE( result );
  TEST_ASSERT_EQUAL( HAWKBIT_DEPLOYMENT_ACTION_FORCED, deployment.update );
  TEST_ASSERT_EQUAL( HAWKBIT_DEPLOYMENT_ACTION_FORCED, deployment.download );
  TEST_ASSERT_EQUAL( 1, deployment.chunkSize );
  TEST_ASSERT_EQUAL_STRING( "os", deployment.chunk[0].part );
  TEST_ASSERT_EQUAL_STRING( "0.1.01", deployment.chunk[0].version );
  TEST_ASSERT_EQUAL_STRING( "TestOS", deployment.chunk[0].name );
  TEST_ASSERT_EQUAL( 1, deployment.chunk[0].artifactsSize );
  TEST_ASSERT_EQUAL_STRING( "main.bin", deployment.chunk[0].artifacts[0].filename );
  TEST_ASSERT_EQUAL_UINT32( 1167968, deployment.chunk[0].artifacts[0].size );
  TEST_ASSERT_EQUAL_STRING( "http://192.168.1.2:8090/DEFAULT/controller/v1/SN6870/softwaremodules/20/artifacts/main.bin", deployment.chunk[0].artifacts[0].download_http );
}

void test_HAWKBITParser_ParseCancelAction( void )
{
  const char* jsonString = "{\"cancelAction\":{\"stopId\":\"42\"}}";
  int actionId;

  bool result = HAWKBITParser_ParseCancelAction( jsonString, &actionId );

  TEST_ASSERT_TRUE( result );
  TEST_ASSERT_EQUAL( 42, actionId );
}
