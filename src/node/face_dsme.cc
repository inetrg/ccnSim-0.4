#include <string.h>
#include <omnetpp.h>

#include "inet_ccn_interest_m.h"
#include "inet/common/packet/Packet.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/networklayer/common/NetworkInterface.h"
#include "inet/linklayer/common/MacAddressTag_m.h"
#include "inet/common/ProtocolTag_m.h"

#include "ccn_data.h"
#include "ccn_interest.h"


    using namespace omnetpp;

    /**
     * Derive the face_dsme class from cSimpleModule.
     */
    class face_dsme : public cSimpleModule
    {
      protected:
        // The following redefined virtual function holds the algorithm.
        virtual void initialize() override;
        virtual void handleMessage(cMessage *msg) override;
    };

    // The module class needs to be registered with OMNeT++
    Define_Module(face_dsme);

    void face_dsme::initialize()
    {
        // Initialize is called at the beginning of the simulation.
        EV << "INIT face_dsme" << "\n";
    }

    void face_dsme::handleMessage(cMessage *msg)
    {
        // EV << msg << "\n";
        // EV << msg->getArrivalGate() << "\n";
        // EV << msg->getArrivalGate()->getIndex() << "\n";
        // EV << msg->getArrivalGate()->getFullName() << "\n";
        // EV << msg->getArrivalGate()->str() << "\n";

        EV << "node["<<getIndex()<<"]: face_dsme::handleMessage\n";

        // simply forward message to the same gate number
        std::string str = msg->getArrivalGate()->getFullName();
        if (str.find("lower") != std::string::npos)
        {
            EV << "RX from lower, send up;" << str  << "\n";
            // find out dst face and send msg there
            send(msg, "upper_layer$o", msg->getArrivalGate()->getIndex());
        }
        else {
            EV << "RX from upper, send down;" << str << "\n";

            char msgName[32];
            int numSent = 666;
            sprintf(msgName, "face_dsme", numSent);
            auto packet = new inet::Packet(msgName);

            switch (msg->getKind())
            {
                case CCN_D:
                    break;

                case CCN_I:
                {
                    ccn_interest *tmp_int = (ccn_interest *)msg;
                    auto payload = inet::makeShared<inet::inet_ccn_interest>();
                    payload->setChunk(tmp_int->getChunk());
                    /* TODO: Chunk shouldn't be zero, otherwise debug mode fails */
                    payload->setChunkLength(inet::B(1));

                    packet->insertAtBack(payload);
                    break;
                }
                default:
                    EV << "UNKNOWN MSG TYPE";
            }

                // take out face index as index for dst host
                int index = msg->getArrivalGate()->getIndex();

                // get network interface of the host assiciated with this node
                inet::NetworkInterface *netif = check_and_cast<inet::NetworkInterface*>(getParentModule()->gate("data_face$o")->getNextGate()->getOwnerModule()->getSubmodule("wlan", 0));

                // get network interface of the host associated to the node we want to reach (dest)
                inet::NetworkInterface *dest_netif = check_and_cast<inet::NetworkInterface*>(getParentModule()->gate("face$o",index+1)->getNextGate()->getOwnerModule()->gate("data_face$o")->getNextGate()->getOwnerModule()->getSubmodule("wlan", 0));

                // tag with associated host interface
                packet->addTag<inet::InterfaceReq>()->setInterfaceId(netif->getInterfaceId());

                // tag with remote host (dest) MAC address
                packet->addTag<inet::MacAddressReq>()->setDestAddress(dest_netif->getMacAddress());

                // tag with local host source address
                packet->addTagIfAbsent<inet::MacAddressReq>()->setSrcAddress(netif->getMacAddress());

                packet->addTag<inet::PacketProtocolTag>()->setProtocol(&inet::Protocol::arp);

                send(packet, "lower_layer$o");
                delete msg;
                // send(msg, "lower_layer$o", msg->getArrivalGate()->getIndex());
        }
    }
