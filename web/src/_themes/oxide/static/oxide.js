/*global Sentry, ackeeTracker*/
var dnt =
    (window.doNotTrack ||
        navigator.doNotTrack ||
        navigator.msDoNotTrack ||
        "msTrackingProtectionEnabled" in window.external) &&
    (window.doNotTrack === "1" ||
        navigator.doNotTrack === "yes" ||
        navigator.doNotTrack === "1" ||
        navigator.msDoNotTrack === "1" ||
        window.external.msTrackingProtectionEnabled());
ackeeTracker
    .create("https://peek.eeems.website", { detailed: !dnt })
    .record("26d065f3-ef1d-44de-8292-5db06a5d4514");
if (!dnt) {
    Sentry.init({
        dsn: "https://0151bab90e7048ccb0e5b138da50c6bb@sentry.eeems.codes/1",
        integrations: [new Sentry.Integrations.BrowserTracing()],
        tracesSampleRate: 1.0,
    });
}
