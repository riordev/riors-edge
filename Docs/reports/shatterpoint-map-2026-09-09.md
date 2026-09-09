# Shatterpoint mission visibility

Shatterpoint still requires district supplies, so its three cache markers now remain visible and trackable before proximity discovery. The other four prototype regions retain optional supply caches and their actual hunt, recorder, uplink or escort objectives. Stable marker identities and post-opening retirement are unchanged.

The existing native destination fixture now exercises IsVisible, Track and GetTrackedMarker against actual fresh-arrival cache actors. It checks every city cache before discovery, verifies hidden optional caches cannot be tracked prematurely, and verifies each other region's actual mission marker stays visible/trackable. Existing native guard death, cache opening, physical reward pickup and marker-retirement coverage remains.

Both 1280x720 frames from seat/captures/shatterpoint-map-1257 were inspected. Broken Avenue, Civic Square and Overpass Market appear as three gold objective markers with readable list cards and distances at fresh arrival. No synthetic discoveries or progress were supplied. Static capture does not demonstrate mouse selection; the native tracking API is exercised separately.
