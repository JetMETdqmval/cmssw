/*
 * DataPoint.h
 *
 *  Created on: Sep 24, 2012
 *      Author: aspataru
 */

#ifndef DATAPOINT_H_
#define DATAPOINT_H_

#include "EventFilter/Utilities/interface/JsonMonitorable.h"
#include "EventFilter/Utilities/interface/JsonSerializable.h"

#include <string>
#include <vector>
#include <memory>
#include <stdint.h>
#include <assert.h>

//#include <tbb/concurrent_vector.h>

namespace jsoncollector {
class DataPoint: public JsonSerializable {

public:

	DataPoint() { }

	DataPoint(std::string const& source, std::string const& definition) :	source_(source), definition_(definition) { }

	//TODO: expected/maxUpdates still useful ? = unsigned int expectedUpdates = 1, unsigned int maxUpdates = 0

	/**
	 * JSON serialization procedure for this class
	 */

	virtual void serialize(Json::Value& root) const;

	/**
	 * JSON deserialization procedure for this class
	 */
	virtual void deserialize(Json::Value& root);

	std::vector<std::string>& getData() {
		return data_;
	}

	/**
	 * Functions specific to new monitoring implementation
	 */

	void serialize(Json::Value& root, bool rootInit, std::string const& input) const;

	//take new update for lumi
	void snap(unsigned int lumi,bool isGlobalUpdate=false);

	//set to track a variable
	void trackMonitorable(JsonMonitorable *monitorable,bool NAifZeroUpdates);

	//set to track a vector of variables
	void trackVector(std::string const& name, std::vector<unsigned int> *inputsPtr, bool NAifZeroUpdates);

	//variable not found by the service, but want to output something to JSON
	void trackDummy(std::string const& name, bool setNAifZeroUpdates)
	{
		isDummy_=true;
		setNAifZeroUpdates_=true;
	}

	//sets which operation is going to be performed on data (specified by JSON def)
	void setOperation(OperationType opType);

	//only used if per-stream DP
	void setStreamLumiPtr(std::vector<std::atomic<unsigned int>> *streamLumiPtr) {
	  streamLumiPtr_=streamLumiPtr;
	}

	//pointed object should be available until discard
	JsonMonitorable * mergeAndRetrieve(unsigned int forLumi);

	//uses move semantics in C++2011
	std::string mergeAndSerialize(JsonValue& jsonRoot, bool initRoot);

	void discardCollected(unsigned int forLumi);

/*
	unsigned int getUpdates() {return updates_;}

*/
	// JSON field names
	static const std::string SOURCE;
	static const std::string DEFINITION;
	static const std::string DATA;

protected:
	//for simple usage
	std::string source_;
	std::string definition_;
	std::vector<std::string> data_;

	//per stream queue of pointers to mon collectors
	std::vector<std::queue<std::auto_ptr<JsonMonitorable>>> dataNative_;
	//lumi for each queue entry
	std::vector<std::queue<unsigned int>> queuedStreamLumi_;

	bool isStream_ = false;
	bool isAtomic_ = false;
	bool isDummy_ = false;
	bool NAifZeroUpdates_ = false;
	
	MonType varType;
	OperationType opType;
	std::vector<std::atomic<unsigned int>> *streamLumiPtr_ = nullptr;

};
}

#endif /* DATAPOINT_H_ */
