#include "../GPT_NRF2401/pp_reply_mailbox.h"
#include <assert.h>
#include <stdint.h>

#define PP_REPLY_TEST_SESSION 0x6A31U
#define PP_REPLY_TEST_OTHER_SESSION 0x6A32U
#define PP_REPLY_TEST_DATA_LENGTH PairedFrame::MAX_BODY
#define PP_REPLY_TEST_CONTROL_LENGTH 7U
#define PP_REPLY_TEST_FIRST_SEQUENCE 0x14U
#define PP_REPLY_TEST_SECOND_SEQUENCE 0x15U
#define PP_REPLY_TEST_FIRST_RETRIES 2U
#define PP_REPLY_TEST_SECOND_RETRIES 3U

struct ReplyOutcome {
	uint8_t sequence;
	uint8_t retries;
	ReplyOutcome():sequence(0),retries(0){}
};

static uint8_t createFrame(uint8_t* frame,uint16_t sid,
	PairedFrame::Kind kind,uint8_t length){
	PairedFrame::Header header;
	header.sessionId=sid;
	header.flags=static_cast<uint8_t>(kind);
	header.txSeq=PP_REPLY_TEST_FIRST_SEQUENCE;
	uint8_t body[PairedFrame::MAX_BODY]={};
	uint8_t width=0;
	assert(PairedFrame::encode(header,body,length,frame,width)==
		PairedFrame::Error::OK);
	return width;
}

int main(){
	typedef PpReplyMailbox<ReplyOutcome> Mailbox;
	uint8_t frame[PairedFrame::MAX_FRAME]={};
	const uint8_t dataWidth=createFrame(frame,PP_REPLY_TEST_SESSION,
		PairedFrame::Kind::DATA,PP_REPLY_TEST_DATA_LENGTH);
	assert(Mailbox::canDefer(true,PP_REPLY_TEST_SESSION,frame,dataWidth));
	assert(!Mailbox::canDefer(false,PP_REPLY_TEST_SESSION,frame,dataWidth));
	assert(!Mailbox::canDefer(true,PP_REPLY_TEST_OTHER_SESSION,frame,dataWidth));
	assert(!Mailbox::canDefer(true,PP_REPLY_TEST_SESSION,frame,0));

	const uint8_t idleWidth=createFrame(frame,PP_REPLY_TEST_SESSION,
		PairedFrame::Kind::IDLE,0);
	assert(Mailbox::canDefer(true,PP_REPLY_TEST_SESSION,frame,idleWidth));
	const uint8_t controlWidth=createFrame(frame,0,
		PairedFrame::Kind::CONTROL,PP_REPLY_TEST_CONTROL_LENGTH);
	assert(!Mailbox::canDefer(true,PP_REPLY_TEST_SESSION,frame,controlWidth));
	const uint8_t otherWidth=createFrame(frame,PP_REPLY_TEST_OTHER_SESSION,
		PairedFrame::Kind::DATA,PP_REPLY_TEST_DATA_LENGTH);
	assert(!Mailbox::canDefer(true,PP_REPLY_TEST_SESSION,frame,otherWidth));

	Mailbox mailbox;
	ReplyOutcome first;
	first.sequence=PP_REPLY_TEST_FIRST_SEQUENCE;
	first.retries=PP_REPLY_TEST_FIRST_RETRIES;
	ReplyOutcome second;
	second.sequence=PP_REPLY_TEST_SECOND_SEQUENCE;
	second.retries=PP_REPLY_TEST_SECOND_RETRIES;
	ReplyOutcome delivered;
	uint8_t width=0;
	assert(!mailbox.take(delivered,width));
	assert(mailbox.store(first,dataWidth));
	assert(mailbox.pending());
	// A second radio RX would overwrite the single shared 32-byte buffer.
	assert(!mailbox.store(second,otherWidth));
	assert(mailbox.take(delivered,width));
	assert(!mailbox.pending());
	assert(delivered.sequence==PP_REPLY_TEST_FIRST_SEQUENCE);
	assert(delivered.retries==PP_REPLY_TEST_FIRST_RETRIES);
	assert(width==dataWidth);
	assert(mailbox.store(second,otherWidth));
	assert(mailbox.take(delivered,width));
	assert(delivered.sequence==PP_REPLY_TEST_SECOND_SEQUENCE);
	assert(delivered.retries==PP_REPLY_TEST_SECOND_RETRIES);
	assert(width==otherWidth);
}
