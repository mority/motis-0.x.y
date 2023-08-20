// automatically generated by the FlatBuffers compiler, do not modify

package motis;

public final class MsgContent {
  private MsgContent() { }
  public static final byte NONE = 0;
  public static final byte MotisNoMessage = 1;
  public static final byte MotisError = 2;
  public static final byte MotisSuccess = 3;
  public static final byte HTTPRequest = 4;
  public static final byte HTTPResponse = 5;
  public static final byte ApiDescription = 6;
  public static final byte FileEvent = 7;
  public static final byte OSMEvent = 8;
  public static final byte ScheduleEvent = 9;
  public static final byte DEMEvent = 10;
  public static final byte PPREvent = 11;
  public static final byte OSRMEvent = 12;
  public static final byte CoastlineEvent = 13;
  public static final byte Connection = 14;
  public static final byte TripId = 15;
  public static final byte AddressRequest = 16;
  public static final byte AddressResponse = 17;
  public static final byte GBFSRoutingRequest = 18;
  public static final byte GBFSRoutingResponse = 19;
  public static final byte StationGuesserRequest = 23;
  public static final byte StationGuesserResponse = 24;
  public static final byte IntermodalRoutingRequest = 25;
  public static final byte LookupGeoStationIdRequest = 26;
  public static final byte LookupGeoStationRequest = 27;
  public static final byte LookupGeoStationResponse = 28;
  public static final byte LookupBatchGeoStationRequest = 29;
  public static final byte LookupBatchGeoStationResponse = 30;
  public static final byte LookupStationEventsRequest = 31;
  public static final byte LookupStationEventsResponse = 32;
  public static final byte LookupScheduleInfoResponse = 33;
  public static final byte LookupMetaStationRequest = 34;
  public static final byte LookupMetaStationResponse = 35;
  public static final byte LookupBatchMetaStationRequest = 36;
  public static final byte LookupBatchMetaStationResponse = 37;
  public static final byte LookupIdTrainRequest = 38;
  public static final byte LookupIdTrainResponse = 39;
  public static final byte OSRMOneToManyRequest = 40;
  public static final byte OSRMOneToManyResponse = 41;
  public static final byte OSRMViaRouteRequest = 42;
  public static final byte OSRMViaRouteResponse = 43;
  public static final byte OSRMSmoothViaRouteRequest = 44;
  public static final byte OSRMSmoothViaRouteResponse = 45;
  public static final byte ParkingGeoRequest = 46;
  public static final byte ParkingGeoResponse = 47;
  public static final byte ParkingLookupRequest = 48;
  public static final byte ParkingLookupResponse = 49;
  public static final byte ParkingEdgeRequest = 50;
  public static final byte ParkingEdgeResponse = 51;
  public static final byte ParkingEdgesRequest = 52;
  public static final byte ParkingEdgesResponse = 53;
  public static final byte PathBoxesResponse = 54;
  public static final byte PathByTripIdRequest = 55;
  public static final byte PathSeqResponse = 56;
  public static final byte PathByStationSeqRequest = 57;
  public static final byte PathByTileFeatureRequest = 58;
  public static final byte MultiPathSeqResponse = 59;
  public static final byte PathByTripIdBatchRequest = 60;
  public static final byte PathByTripIdBatchResponse = 61;
  public static final byte FootRoutingRequest = 62;
  public static final byte FootRoutingResponse = 63;
  public static final byte FootRoutingSimpleRequest = 64;
  public static final byte FootRoutingSimpleResponse = 65;
  public static final byte FootRoutingProfilesResponse = 66;
  public static final byte RailVizMapConfigResponse = 67;
  public static final byte RailVizTrainsRequest = 68;
  public static final byte RailVizTrainsResponse = 69;
  public static final byte RailVizTripsRequest = 70;
  public static final byte RailVizStationRequest = 71;
  public static final byte RailVizStationResponse = 72;
  public static final byte RailVizTripGuessRequest = 73;
  public static final byte RailVizTripGuessResponse = 74;
  public static final byte ReviseRequest = 75;
  public static final byte ReviseResponse = 76;
  public static final byte RISBatch = 77;
  public static final byte UpdatedEvent = 78;
  public static final byte Event = 79;
  public static final byte RISMessage = 80;
  public static final byte RISGTFSRTMapping = 81;
  public static final byte RISForwardTimeRequest = 82;
  public static final byte RISPurgeRequest = 83;
  public static final byte RoutingRequest = 84;
  public static final byte RoutingResponse = 85;
  public static final byte RtEventInfo = 86;
  public static final byte RtDelayUpdate = 87;
  public static final byte RtTrackUpdate = 88;
  public static final byte RtFreeTextUpdate = 89;
  public static final byte RtRerouteUpdate = 90;
  public static final byte RtUpdate = 91;
  public static final byte RtUpdates = 92;
  public static final byte RtWriteGraphRequest = 93;
  public static final byte TripBasedTripDebugRequest = 94;
  public static final byte TripBasedTripDebugResponse = 95;
  public static final byte PaxMonUpdate = 96;
  public static final byte PaxForecastUpdate = 97;
  public static final byte PaxMonAddGroupsRequest = 98;
  public static final byte PaxMonAddGroupsResponse = 99;
  public static final byte PaxMonRemoveGroupsRequest = 100;
  public static final byte PaxMonTripLoadInfo = 101;
  public static final byte PaxMonFindTripsRequest = 102;
  public static final byte PaxMonFindTripsResponse = 103;
  public static final byte PaxMonStatusResponse = 104;
  public static final byte PaxMonGetGroupsRequest = 105;
  public static final byte PaxMonGetGroupsResponse = 106;
  public static final byte PaxMonFilterGroupsRequest = 107;
  public static final byte PaxMonFilterGroupsResponse = 108;
  public static final byte PaxMonFilterTripsRequest = 109;
  public static final byte PaxMonFilterTripsResponse = 110;
  public static final byte PaxMonGetTripLoadInfosRequest = 111;
  public static final byte PaxMonGetTripLoadInfosResponse = 112;
  public static final byte PaxMonForkUniverseRequest = 113;
  public static final byte PaxMonForkUniverseResponse = 114;
  public static final byte PaxMonDestroyUniverseRequest = 115;
  public static final byte PaxMonGetGroupsInTripRequest = 116;
  public static final byte PaxMonGetGroupsInTripResponse = 117;
  public static final byte PaxForecastApplyMeasuresRequest = 118;
  public static final byte PaxMonUniverseForked = 119;
  public static final byte PaxMonUniverseDestroyed = 120;
  public static final byte PaxMonGetInterchangesRequest = 121;
  public static final byte PaxMonGetInterchangesResponse = 122;
  public static final byte PaxMonStatusRequest = 123;
  public static final byte RISApplyRequest = 124;
  public static final byte RISSystemTimeChanged = 125;
  public static final byte RtGraphUpdated = 126;
  public static final byte LookupRiBasisRequest = 127;

  private static final String[] names = { "NONE", "MotisNoMessage", "MotisError", "MotisSuccess", "HTTPRequest", "HTTPResponse", "ApiDescription", "FileEvent", "OSMEvent", "ScheduleEvent", "DEMEvent", "PPREvent", "OSRMEvent", "CoastlineEvent", "Connection", "TripId", "AddressRequest", "AddressResponse", "GBFSRoutingRequest", "GBFSRoutingResponse", "", "", "", "StationGuesserRequest", "StationGuesserResponse", "IntermodalRoutingRequest", "LookupGeoStationIdRequest", "LookupGeoStationRequest", "LookupGeoStationResponse", "LookupBatchGeoStationRequest", "LookupBatchGeoStationResponse", "LookupStationEventsRequest", "LookupStationEventsResponse", "LookupScheduleInfoResponse", "LookupMetaStationRequest", "LookupMetaStationResponse", "LookupBatchMetaStationRequest", "LookupBatchMetaStationResponse", "LookupIdTrainRequest", "LookupIdTrainResponse", "OSRMOneToManyRequest", "OSRMOneToManyResponse", "OSRMViaRouteRequest", "OSRMViaRouteResponse", "OSRMSmoothViaRouteRequest", "OSRMSmoothViaRouteResponse", "ParkingGeoRequest", "ParkingGeoResponse", "ParkingLookupRequest", "ParkingLookupResponse", "ParkingEdgeRequest", "ParkingEdgeResponse", "ParkingEdgesRequest", "ParkingEdgesResponse", "PathBoxesResponse", "PathByTripIdRequest", "PathSeqResponse", "PathByStationSeqRequest", "PathByTileFeatureRequest", "MultiPathSeqResponse", "PathByTripIdBatchRequest", "PathByTripIdBatchResponse", "FootRoutingRequest", "FootRoutingResponse", "FootRoutingSimpleRequest", "FootRoutingSimpleResponse", "FootRoutingProfilesResponse", "RailVizMapConfigResponse", "RailVizTrainsRequest", "RailVizTrainsResponse", "RailVizTripsRequest", "RailVizStationRequest", "RailVizStationResponse", "RailVizTripGuessRequest", "RailVizTripGuessResponse", "ReviseRequest", "ReviseResponse", "RISBatch", "UpdatedEvent", "Event", "RISMessage", "RISGTFSRTMapping", "RISForwardTimeRequest", "RISPurgeRequest", "RoutingRequest", "RoutingResponse", "RtEventInfo", "RtDelayUpdate", "RtTrackUpdate", "RtFreeTextUpdate", "RtRerouteUpdate", "RtUpdate", "RtUpdates", "RtWriteGraphRequest", "TripBasedTripDebugRequest", "TripBasedTripDebugResponse", "PaxMonUpdate", "PaxForecastUpdate", "PaxMonAddGroupsRequest", "PaxMonAddGroupsResponse", "PaxMonRemoveGroupsRequest", "PaxMonTripLoadInfo", "PaxMonFindTripsRequest", "PaxMonFindTripsResponse", "PaxMonStatusResponse", "PaxMonGetGroupsRequest", "PaxMonGetGroupsResponse", "PaxMonFilterGroupsRequest", "PaxMonFilterGroupsResponse", "PaxMonFilterTripsRequest", "PaxMonFilterTripsResponse", "PaxMonGetTripLoadInfosRequest", "PaxMonGetTripLoadInfosResponse", "PaxMonForkUniverseRequest", "PaxMonForkUniverseResponse", "PaxMonDestroyUniverseRequest", "PaxMonGetGroupsInTripRequest", "PaxMonGetGroupsInTripResponse", "PaxForecastApplyMeasuresRequest", "PaxMonUniverseForked", "PaxMonUniverseDestroyed", "PaxMonGetInterchangesRequest", "PaxMonGetInterchangesResponse", "PaxMonStatusRequest", "RISApplyRequest", "RISSystemTimeChanged", "RtGraphUpdated", "LookupRiBasisRequest", "LookupRiBasisResponse", "PaxForecastApplyMeasuresResponse", "PaxMonGetAddressableGroupsRequest", "PaxMonGetAddressableGroupsResponse", "OSRMManyToManyRequest", "OSRMManyToManyResponse", "GBFSProvidersResponse", "PaxMonKeepAliveRequest", "PaxMonKeepAliveResponse", "PaxMonRerouteGroupsRequest", "PaxMonRerouteGroupsResponse", "PaxMonGroupStatisticsRequest", "PaxMonGroupStatisticsResponse", "PaxMonDebugGraphRequest", "PaxMonDebugGraphResponse", "PaxMonGetUniversesResponse", "RISApplyResponse", "LookupStationInfoRequest", "LookupStationInfoResponse", "NigiriEvent", "PaxMonGetTripCapacityRequest", "PaxMonGetTripCapacityResponse", "RtMessageHistoryRequest", "RtMessageHistoryResponse", "LookupStationLocationResponse", "InputStation", "StationsEvent", };

  public static String name(int e) { return names[e]; }
};

