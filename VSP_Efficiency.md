# AqualinkD / AquaDaemon Flow Efficiency Monitoring

The Flow Efficiency feature tracks the hydraulic performance of your pool's plumbing system in real-time. By monitoring how much electrical power your pump consumes relative to its rotational speed, the system can dynamically detect flow restrictions. This acts as an automated diagnostic tool, letting you know exactly when your filter requires backwashing/cleaning, or when your skimmer and pump baskets are clogged.

> ⚠️ **CRITICAL REQUIREMENT:** This feature **ONLY** supports variable speed pumps (VSPs) that natively report **real, measured RPM and Watts** back over the RS485 serial bus (e.g., Pentair IntelliFlo VS, Jandy ePUMP). It will **NOT** work with pumps that report estimated or mathematically calculated values, or systems missing real-time wattage feedback.

---

## Technical Overview & Core Rules

1. **Establish a Clean System Baseline First**
   Before capturing configuration values, you must completely clean your pool's filtration network. Backwash or clean your main filter, empty the pump strainer basket, and clear out all skimmer baskets. The baseline values must represent your plumbing at its absolute peak performance.
2. **Minimum Operational Limit (1000 RPM)**
   The fluid dynamics math relies on the pump moving water effectively. AqualinkD enforces a hard floor at **1000 RPM**. Do not configure or test baseline speeds below 1000 RPM, as affinity equations disintegrate at ultra-low speeds where motor overhead dominates power consumption.
3. **Bracket Your Operational Speeds**
   For the best results, pick three speeds (`Low`, `Med`, `High`) that match your actual daily schedules. For example, if you run a low speed for overnight filtration, a medium speed for skimming, and a high speed for a pool cleaner, use those exact RPM steps as your anchors.

---

## Step 1: Gather Your Clean Baseline Data

Once your filters and baskets are completely clean, manually run your pump at your three chosen operational speeds. Let the flow stabilize for 60 seconds at each step, then record the live **RPM** and **Watts** reported by AqualinkD or your control panel. (Set the Debug Mask `PentaDvce` or `JandyDvce` in `Aqmanager` to get live readings)

**Example Real-World Observations:**
* **Low Speed:** 1250 RPM $\rightarrow$ Draws **124 Watts**
* **Medium Speed:** 1750 RPM $\rightarrow$ Draws **289 Watts**
* **High Speed:** 2750 RPM $\rightarrow$ Draws **1130 Watts**

---

## Step 2: Calculate Your Scaled K Values

Because raw fluid dynamic constants are microscopically small decimals (e.g., `0.00000005392`), AqualinkD uses a **Scaled K** value multiplied by **1 Billion ($1,000,000,000$)**. This provides clean, human-readable numbers in logs and MQTT topics.

For each of your three speeds, use the following formula:

$$\text{Scaled K} = \left( \frac{\text{Watts}}{\text{RPM}^3} \right) \times 1,000,000,000$$

### Example Math Walkthrough:

* **Low Speed (1250 RPM @ 124W):**
  $$1250^3 = 1,953,125,000$$
  $$\left( \frac{124}{1,953,125,000} \right) \times 1,000,000,000 = \mathbf{63.49}$$

* **Medium Speed (1750 RPM @ 289W):**
  $$1750^3 = 5,359,375,000$$
  $$\left( \frac{289}{5,359,375,000} \right) \times 1,000,000,000 = \mathbf{53.92}$$

* **High Speed (2750 RPM @ 1130W):**
  $$2750^3 = 20,796,875,000$$
  $$\left( \frac{1130}{20,796,875,000} \right) \times 1,000,000,000 = \mathbf{54.34}$$

---

## Step 3: Update Your Configuration File

Open your `aqualinkd.conf` file and locate your pump configuration block. Add your target speeds and calculated constants to your primary pump settings (replace `01` with your pump's actual button index):

```ini
# ==============================================================================
# PUMP FLOW EFFICIENCY PROFILE
# ==============================================================================
# Low Speed Anchor
button_01_pumpBaselineRpmLow=1250
button_01_pumpBaselineKLow=63.49

# Medium Speed Anchor
button_01_pumpBaselineRpmMed=1750
button_01_pumpBaselineKMed=53.92

# High Speed Anchor
button_01_pumpBaselineRpmHigh=2750
button_01_pumpBaselineKHigh=54.34
```

> ⚠️ **WARNING:** This feature is not supported in Aqmanager's config editor yet.  Please do not use AqualinkD's UI to modify your configuration as these values will be lost.<br>***Manually edit aqualinkd.conf*** and keep it up to date with manual edits.


## Step 4: Update AqualinkD web UI to give warnings.

You can use the below to configure AqualinkD's web UI so that is warns you with red tile and displays descriptive text. In the below example anything below 90% efficiency threshold is used for warning.
```
"Sensor/Aux_S5": {
      "outofrange": {
        "min": 90,
        "mintext": "Clean Filter(s)"
      }
    }
```

In larger context of where this needs to sit
```diff
"tile_thresholds": {
    "SWG/PPM": {
      "outofrange": {
        "min": 2600,
        "max": 3500,
        "mintext": "Add Salt"
      },
      "attention": {
        "min": 2700,
        "max": 3400,
        "mintext": "Add Salt"
      }
    },
    "CHEM/pH": {
      "outofrange": {
        "min": 7,
        "max": 8
      },
      "attention": {
        "min": 7.2,
        "max": 7.8,
        "mintext": "Low",
        "maxtext": "High"
      }
    },
    "CHEM/ORP": {
      "outofrange": {
        "min": 560,
        "max": 900
      },
      "attention": {
        "min": 650,
        "max": 850,
        "mintext": "Low",
        "maxtext": "High"
      }
    },
+    "Sensor/Aux_S5": {
+      "outofrange": {
+        "min": 90,
+        "mintext": "Clean Filter(s)"
+      }
+    }
  },
  "swg_status": {
    "0": "On",
    "1": "No flow",
    "2": "Low salt",
    "4": "High salt",
    "8": "Clean cell",
    "9": "Turning off",
    "16": "High current",
    "32": "Low volts",
    "64": "Low temp",
    "128": "Check PCB",
    "253": "General Fault",
    "254": "Unknown",
    "255": "Off"
  },
```