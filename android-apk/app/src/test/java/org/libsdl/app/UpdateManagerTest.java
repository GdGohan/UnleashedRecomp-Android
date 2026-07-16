package org.libsdl.app;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

public final class UpdateManagerTest {
    @Test
    public void forkRevisionIsCompared() {
        assertTrue(UpdateManager.compareVersions("0.5.2-3", "0.5.2-2") > 0);
        assertTrue(UpdateManager.compareVersions("v0.5.2-3", "0.5.2") > 0);
    }

    @Test
    public void upstreamPatchStillHasPriority() {
        assertTrue(UpdateManager.compareVersions("0.5.3", "0.5.2-99") > 0);
    }

    @Test
    public void equivalentVersionsCompareEqual() {
        assertEquals(0, UpdateManager.compareVersions("v0.5.2-3", "0.5.2-3"));
    }
}
