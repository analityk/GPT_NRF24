#include "../GPT_NRF2401/spi_block_transfer.h"
#include <assert.h>
#include <stdint.h>

#define SPI_TEST_BUFFER_BYTES 32U
#define SPI_TEST_COMMAND 0xA0U
#define SPI_TEST_STATUS 0x0EU
#define SPI_TEST_READ_COMMAND 0x61U
#define SPI_TEST_PATTERN_OFFSET 0x25U
#define SPI_TEST_WAIT_POLLS 3U
#define SPI_TEST_FIRST_DATA_INDEX 1U
#define SPI_TEST_SECOND_DATA_INDEX 2U
#define SPI_TEST_NEVER_FAIL 0xFFU
#define SPI_TEST_MISSING_MASTER 0U

struct FakeSpiBus {
	bool enabled;
	uint8_t writes[SPI_TEST_BUFFER_BYTES+SPI_TEST_FIRST_DATA_INDEX];
	uint8_t reply[SPI_TEST_BUFFER_BYTES+SPI_TEST_FIRST_DATA_INDEX];
	uint8_t transfers;
	uint8_t polls;
	uint8_t failAt;
	uint16_t totalPolls;
	uint8_t reads;

	FakeSpiBus():enabled(true),transfers(0),polls(0),
		failAt(SPI_TEST_NEVER_FAIL),totalPolls(0),reads(0){
		for(uint8_t i=0;i<sizeof(reply);++i){
			reply[i]=static_cast<uint8_t>(i+SPI_TEST_PATTERN_OFFSET);
		}
		reply[0]=SPI_TEST_STATUS;
	}

	bool master() const{
		return enabled;
	}

	void write(uint8_t value){
		writes[transfers]=value;
		polls=0;
		++transfers;
	}

	bool ready(){
		++polls;
		++totalPolls;
		if(static_cast<uint8_t>(transfers-1U)==failAt){
			return false;
		}
		return polls>=SPI_TEST_WAIT_POLLS;
	}

	uint8_t read(){
		++reads;
		return reply[transfers-1U];
	}
};

int main(){
	typedef SpiBlockTransfer<FakeSpiBus> Transfer;
	uint8_t tx[SPI_TEST_BUFFER_BYTES];
	uint8_t rx[SPI_TEST_BUFFER_BYTES];
	for(uint8_t i=0;i<SPI_TEST_BUFFER_BYTES;++i){
		tx[i]=static_cast<uint8_t>(i+SPI_TEST_PATTERN_OFFSET);
		rx[i]=0;
	}
	FakeSpiBus txOnly;
	uint8_t status=0;
	assert(Transfer::exchange(txOnly,SPI_TEST_COMMAND,tx,0,
		SPI_TEST_BUFFER_BYTES,&status));
	assert(status==SPI_TEST_STATUS);
	assert(txOnly.transfers==SPI_TEST_BUFFER_BYTES+
		SPI_TEST_FIRST_DATA_INDEX);
	assert(txOnly.reads==txOnly.transfers);
	for(uint8_t i=0;i<SPI_TEST_BUFFER_BYTES;++i){
		assert(txOnly.writes[i+SPI_TEST_FIRST_DATA_INDEX]==tx[i]);
	}

	FakeSpiBus rxOnly;
	assert(Transfer::exchange(rxOnly,SPI_TEST_READ_COMMAND,0,rx,
		SPI_TEST_BUFFER_BYTES,0));
	for(uint8_t i=0;i<SPI_TEST_BUFFER_BYTES;++i){
		assert(rxOnly.writes[i+SPI_TEST_FIRST_DATA_INDEX]==
			SPI_BLOCK_READ_FILL_BYTE);
		assert(rx[i]==rxOnly.reply[i+SPI_TEST_FIRST_DATA_INDEX]);
	}

	FakeSpiBus duplex;
	assert(Transfer::exchange(duplex,SPI_TEST_COMMAND,tx,rx,
		SPI_TEST_BUFFER_BYTES,0));
	for(uint8_t i=0;i<SPI_TEST_BUFFER_BYTES;++i){
		assert(duplex.writes[i+SPI_TEST_FIRST_DATA_INDEX]==tx[i]);
		assert(rx[i]==duplex.reply[i+SPI_TEST_FIRST_DATA_INDEX]);
	}

	FakeSpiBus dummy;
	assert(Transfer::exchange(dummy,SPI_TEST_READ_COMMAND,0,0,
		SPI_TEST_BUFFER_BYTES,0));
	assert(dummy.transfers==SPI_TEST_BUFFER_BYTES+
		SPI_TEST_FIRST_DATA_INDEX);

	FakeSpiBus noMaster;
	noMaster.enabled=false;
	assert(!Transfer::exchange(noMaster,SPI_TEST_COMMAND,tx,rx,
		SPI_TEST_BUFFER_BYTES,0));
	assert(noMaster.transfers==SPI_TEST_MISSING_MASTER);

	FakeSpiBus commandTimeout;
	commandTimeout.failAt=0;
	status=SPI_TEST_PATTERN_OFFSET;
	assert(!Transfer::exchange(commandTimeout,SPI_TEST_COMMAND,tx,rx,
		SPI_TEST_BUFFER_BYTES,&status));
	assert(status==SPI_TEST_PATTERN_OFFSET);
	assert(commandTimeout.reads==0U);
	assert(commandTimeout.totalPolls==SPI_BLOCK_BYTE_TIMEOUT_POLLS);

	FakeSpiBus dataTimeout;
	dataTimeout.failAt=SPI_TEST_SECOND_DATA_INDEX;
	assert(!Transfer::exchange(dataTimeout,SPI_TEST_COMMAND,tx,rx,
		SPI_TEST_BUFFER_BYTES,0));
	assert(dataTimeout.transfers==SPI_TEST_SECOND_DATA_INDEX+
		SPI_TEST_FIRST_DATA_INDEX);
	assert(dataTimeout.reads==SPI_TEST_SECOND_DATA_INDEX);
	assert(dataTimeout.totalPolls==
		SPI_TEST_SECOND_DATA_INDEX*SPI_TEST_WAIT_POLLS+
		SPI_BLOCK_BYTE_TIMEOUT_POLLS);
}
