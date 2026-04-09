SET(HEADERS
  FollowObject.h
)

SET(SOURCES
  FollowObject.cpp
)

ADD_COVISE_MODULE(IO FollowObjectModule ${HEADERS} ${SOURCES})
SET_TARGET_PROPERTIES(FollowObjectModule PROPERTIES OUTPUT_NAME "FollowObject")
TARGET_LINK_LIBRARIES(FollowObjectModule coApi coAppl coCore coDo coUtil)
COVISE_INSTALL_TARGET(FollowObjectModule)
