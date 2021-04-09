#include <string.h>
#include <omnetpp.h>

#include "inet_ccn_interest_m.h"
#include "inet_ccn_data_m.h"
#include "ccnsim_inet_info_m.h"
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
        inet::Packet *ccn_msg_to_inet(cMessage *msg, int dest_face);
        cMessage *inet_to_ccn_msg(inet::Packet *packet, int *dest_face);
        template <typename T, typename S> void copy_ccn_data(T origin, S dest);
        template <typename T, typename S> void copy_ccn_interest(T origin, S dest);
    };

    // The module class needs to be registered with OMNeT++
    Define_Module(face_dsme);

    void face_dsme::initialize()
    {
        // Initialize is called at the beginning of the simulation.
        EV << "INIT face_dsme" << "\n";
    }

    template <typename T, typename S> void face_dsme::copy_ccn_data(T origin, S dest)
    {
        dest->setChunk(origin->getChunk());
        dest->setPrice(origin->getPrice());
        dest->setTarget(origin->getTarget());
        dest->setOrigin(origin->getOrigin());
        dest->setHops(origin->getHops());
        dest->setTSB(origin->getTSB());
        dest->setTSI(origin->getTSI());
        dest->setCapacity(origin->getCapacity());
        dest->setBtw(origin->getBtw());
        dest->setFound(origin->getFound());
    }

    template <typename T, typename S> void face_dsme::copy_ccn_interest(T origin, S dest)
    {
        dest->setPathArraySize(origin->getPathArraySize());
        for (int k=0; k < origin->getPathArraySize(); k++) {
            dest->setPath(k, origin->getPath(k));
        }
        dest->setChunk(origin->getChunk());
        dest->setHops(origin->getHops());
        dest->setTarget(origin->getTarget());
        dest->setRep_target(origin->getRep_target());
        dest->setBtw(origin->getBtw());
        dest->setTTL(origin->getTTL());
        dest->setNfound(origin->getNfound());
        dest->setCapacity(origin->getCapacity());
        dest->setOrigin(origin->getOrigin());
        dest->setDelay(origin->getDelay());
        dest->setSerialNumber(origin->getSerialNumber());
        dest->setAggregate(origin->getAggregate());
    }

    cMessage *face_dsme::inet_to_ccn_msg(inet::Packet *packet, int *dest_face)
    {
        cMessage *msg;
        auto info = packet->removeAtFront<inet::ccnsim_inet_info>();
        switch(info->getType()) {
            case CCN_D: {
                auto payload = packet->removeAtFront<inet::inet_ccn_data>();
                /* Create and populate CCN interest */
                msg = new ccn_data("data",CCN_D);

                /* Create and populate CCN data */
                ccn_data *data = check_and_cast<ccn_data*>(msg);
                copy_ccn_data<inet::IntrusivePtr<inet::inet_ccn_data>, ccn_data*>(payload, data);

                break;
            }
            case CCN_I: {
                auto payload = packet->removeAtFront<inet::inet_ccn_interest>();
                /* Create and populate CCN interest */
                msg = new ccn_interest("interest",CCN_I);
                ccn_interest *interest = check_and_cast<ccn_interest*>(msg);
                copy_ccn_interest<inet::IntrusivePtr<inet::inet_ccn_interest>, ccn_interest*>(payload, interest);
                break;
            }
            default:
                throw cRuntimeError("inet_to_ccn_msg: Unknown CCN packet type!") ;
        }

        *dest_face = info->getDest_face();
        return msg;
    }

    inet::Packet *face_dsme::ccn_msg_to_inet(cMessage *msg, int dest_face)
    {
        inet::Packet *packet;

        switch (msg->getKind())
        {
            case CCN_D: {
                packet = new inet::Packet("ccn_data");
                ccn_data *tmp_data = (ccn_data *)msg;
                auto payload = inet::makeShared<inet::inet_ccn_data>();

                copy_ccn_data<ccn_data*, inet::IntrusivePtr<inet::inet_ccn_data>>(tmp_data, payload);

                /* TODO: Chunk shouldn't be zero, otherwise debug mode fails */
                payload->setChunkLength(inet::B(1));
                packet->insertAtFront(payload);

                break;
                }

            case CCN_I:
            {
                packet = new inet::Packet("ccn_interest");
                ccn_interest *tmp_int = (ccn_interest *)msg;
                auto payload = inet::makeShared<inet::inet_ccn_interest>();
                copy_ccn_interest<ccn_interest*, inet::IntrusivePtr<inet::inet_ccn_interest>>(tmp_int, payload);


                /* TODO: Chunk shouldn't be zero, otherwise debug mode fails */
                payload->setChunkLength(inet::B(1));
                packet->insertAtFront(payload);
                break;
            }
            default:
                throw cRuntimeError("ccn_msg_to_inet: Unknown CCN packet type!") ;
        }


            auto info = inet::makeShared<inet::ccnsim_inet_info>();
            info->setType(msg->getKind());
            info->setDest_face(getParentModule()->gate("face$o",dest_face)->getNextGate()->getIndex());
            info->setChunkLength(inet::B(1));
            packet->insertAtFront(info);

            return packet;
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
            inet::Packet *packet = check_and_cast<inet::Packet*>(msg);
            int dest_face;

            cMessage *ccn_msg = inet_to_ccn_msg(packet, &dest_face);
            send(ccn_msg, "upper_layer$o", dest_face);
            delete msg;
        }
        else {
            EV << "RX from upper, send down;" << str << "\n";

                int index = msg->getArrivalGate()->getIndex();
                auto packet = ccn_msg_to_inet(msg, index);
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
