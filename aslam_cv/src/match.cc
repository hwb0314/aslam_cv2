#include <aslam/common/stl-helpers.h>
#include <aslam/frames/visual-frame.h>
#include <aslam/frames/visual-nframe.h>
#include <glog/logging.h>

#include "aslam/matcher/match.h"

namespace aslam {

void convertMatches(const MatchesWithScore& matches_with_score_A_B, Matches* matches_A_B) {
  CHECK_NOTNULL(matches_A_B);
  matches_A_B->clear();
  for (const aslam::MatchWithScore& match : matches_with_score_A_B) {
    CHECK_GE(match.getIndexApple(), 0) << "Apple keypoint index is negative.";
    CHECK_GE(match.getIndexBanana(), 0) << "Banana keypoint index is negative.";
    matches_A_B->emplace_back(static_cast<size_t> (match.getIndexApple()),
                              static_cast<size_t> (match.getIndexBanana()));
  }
  CHECK_EQ(matches_with_score_A_B.size(), matches_A_B->size());
}

size_t extractMatchesFromTrackIdChannel(const aslam::VisualFrame& frame_kp1,
                                        const aslam::VisualFrame& frame_k,
                                        aslam::Matches* matches_kp1_kp) {
  CHECK_NOTNULL(matches_kp1_kp);
  CHECK_EQ(frame_kp1.getRawCameraGeometry().get(), frame_k.getRawCameraGeometry().get());
  const Eigen::VectorXi& track_ids_kp1 = frame_kp1.getTrackIds();
  const Eigen::VectorXi& track_ids_k = frame_k.getTrackIds();

  // Build trackid <-> keypoint_idx lookup table.
  typedef std::unordered_map<int, size_t> TrackIdKeypointIdxMap;
  TrackIdKeypointIdxMap track_id_kp1_keypoint_idx_kp1_map;
  for (int keypoint_idx_kp1 = 0; keypoint_idx_kp1 < track_ids_kp1.rows(); ++keypoint_idx_kp1) {
    int track_id_kp1 = track_ids_kp1(keypoint_idx_kp1);
    // Skip unassociated keypoints.
    if(track_id_kp1 < 0)
      continue;
    track_id_kp1_keypoint_idx_kp1_map.insert(std::make_pair(track_id_kp1, keypoint_idx_kp1));
  }
  CHECK_LE(track_id_kp1_keypoint_idx_kp1_map.size(), frame_kp1.getNumKeypointMeasurements());

  // Create indices matche vector using the lookup table.
  matches_kp1_kp->clear();
  for (int keypoint_idx_k = 0; keypoint_idx_k < track_ids_k.rows(); ++keypoint_idx_k) {
    int track_id_k = track_ids_k(keypoint_idx_k);
    if(track_id_k < 0)
      continue;
    TrackIdKeypointIdxMap::const_iterator it = track_id_kp1_keypoint_idx_kp1_map.find(track_id_k);
    if (it != track_id_kp1_keypoint_idx_kp1_map.end()) {
      size_t keypoint_idx_kp1 = it->second;
      matches_kp1_kp->emplace_back(keypoint_idx_kp1, keypoint_idx_k);
    }
  }
  return matches_kp1_kp->size();
}

size_t extractMatchesFromTrackIdChannels(const aslam::VisualNFrame& nframe_kp1,
                                         const aslam::VisualNFrame& nframe_k,
                                         std::vector<aslam::Matches>* rig_matches_kp1_kp) {
  CHECK_NOTNULL(rig_matches_kp1_kp);
  CHECK_EQ(nframe_kp1.getNCameraShared().get(), nframe_k.getNCameraShared().get());

  size_t num_cameras = nframe_kp1.getNumCameras();
  rig_matches_kp1_kp->clear();
  rig_matches_kp1_kp->resize(num_cameras);

  size_t num_matches = 0;
  for (size_t cam_idx = 0; cam_idx < nframe_kp1.getNumCameras(); ++cam_idx) {
    num_matches += extractMatchesFromTrackIdChannel(nframe_kp1.getFrame(cam_idx),
                                                    nframe_k.getFrame(cam_idx),
                                                    &(*rig_matches_kp1_kp)[cam_idx]);
  }
  return num_matches;
}

double getMatchPixelDisparityMedian(const aslam::VisualNFrame& nframe_kp1,
                                    const aslam::VisualNFrame& nframe_k,
                                    const std::vector<aslam::Matches>& matches_kp1_kp) {
  CHECK_EQ(nframe_kp1.getNCameraShared().get(), nframe_k.getNCameraShared().get());

  const size_t num_cameras = nframe_kp1.getNumCameras();
  CHECK_EQ(matches_kp1_kp.size(), num_cameras);
  const size_t num_matches = countRigMatches(matches_kp1_kp);
  std::vector<double> disparity_px(num_matches);

  size_t match_idx = 0;
  for (size_t cam_idx = 0; cam_idx < num_cameras; ++cam_idx) {
    const Eigen::Matrix2Xd& keypoints_kp1 = nframe_kp1.getFrame(cam_idx).getKeypointMeasurements();
    const Eigen::Matrix2Xd& keypoints_k = nframe_k.getFrame(cam_idx).getKeypointMeasurements();
    for (const aslam::Match& match_kp1_kp : matches_kp1_kp[cam_idx]) {
      CHECK_LT(static_cast<int>(match_kp1_kp.first), keypoints_kp1.cols());
      CHECK_LT(static_cast<int>(match_kp1_kp.second), keypoints_k.cols());
      disparity_px[match_idx++] = (keypoints_kp1.col(match_kp1_kp.first)
          - keypoints_k.col(match_kp1_kp.second)).norm();
    }
  }
  CHECK_EQ(match_idx, num_matches);
  return aslam::common::median(disparity_px.begin(), disparity_px.end());
}

}  // namespace aslam
