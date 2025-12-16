# Bugs Found in IOSelectric.ino

## Fixed

### 1. Critical: Logic error in shift checking (Line 740) - FIXED
**Severity:** High
**Location:** Software/IOSelectric/IOSelectric.ino:740

**Issue:** The condition used OR (`||`) when it should use AND (`&&`):
```c
if ((code!=0x1a)|| (code!=0x29)) {
```

This condition always evaluates to true because:
- If `code` is not `0x1a`, the first part is true
- If `code` IS `0x1a`, then it's not `0x29`, so the second part is true

**Fix:** Changed to:
```c
if ((code!=0x1a) && (code!=0x29)) {
```

**Impact:** This bug caused unnecessary shift operations even for "." and "," characters which exist on both shift levels, potentially causing performance issues and mechanical wear.

---

## Remaining Issues (Not Fixed)

### 2. Loop logic error in checkDelay() (Line 1383)
**Severity:** Medium
**Location:** Software/IOSelectric/IOSelectric.ino:1383

**Issue:**
```c
for (int i=del; i--; i>0) {
```

The loop has a nonsensical condition. The `i--` decrements and returns the value (continues while non-zero), but `i>0` is a separate expression that doesn't affect the loop control.

**Suggested Fix:**
```c
for (int i=del; i>0; i--) {
```
or
```c
for (int i=0; i<del; i++) {
```

**Impact:** The loop may not execute the intended number of iterations.

### 3. Type mismatch in unlockKeyboard() (Line 676)
**Severity:** Low
**Location:** Software/IOSelectric/IOSelectric.ino:676

**Issue:**
```c
int TO = start + TO_UNLOCK;
```

`TO` should be `unsigned long` to match the pattern used elsewhere in the code and to match the type of `start` (which is `unsigned long`).

**Suggested Fix:**
```c
unsigned long TO = start + TO_UNLOCK;
```

**Impact:** Potential overflow or comparison issues with millisecond timestamps.

### 4. Infinite loop after timeout (Line 841)
**Severity:** High
**Location:** Software/IOSelectric/IOSelectric.ino:841

**Issue:**
```c
while (true) ;
```

After a timeout waiting for C2 to close, the code enters an infinite loop that hangs the system forever.

**Suggested Fix:** Remove the infinite loop, or replace with a proper error handling mechanism:
```c
// Remove this line, the break statement below will handle the exit
```

**Impact:** System hangs permanently after this timeout condition, requiring a hard reset.

### 5. Duplicate label in displayInputs() (Line 1405)
**Severity:** Low
**Location:** Software/IOSelectric/IOSelectric.ino:1405

**Issue:**
```c
strcat(str,"EOL_NC "); strcat(str,inputState(C_EOL_NC));
strcat(str,"EOL_NC "); strcat(str,inputState(C_EOL_NO));  // Should be "EOL_NO"
```

The second line incorrectly labels the C_EOL_NO contact as "EOL_NC".

**Suggested Fix:**
```c
strcat(str,"EOL_NO "); strcat(str,inputState(C_EOL_NO));
```

**Impact:** Confusing debug output when displaying input states.
