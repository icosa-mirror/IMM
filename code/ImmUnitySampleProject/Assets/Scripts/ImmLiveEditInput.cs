using System;
using System.Reflection;
using UnityEngine;

namespace ImmPlayer
{
    // Optional sample input. Some validation hosts select the new input backend
    // without installing its package; ENABLE_INPUT_SYSTEM alone cannot guard a
    // compile-time Unity.InputSystem dependency in this lab.
    internal sealed class ImmLiveEditInput
    {
        private readonly PropertyInfo keyboardCurrent = Type.GetType("UnityEngine.InputSystem.Keyboard, Unity.InputSystem")?.GetProperty("current");
        private readonly PropertyInfo mouseCurrent = Type.GetType("UnityEngine.InputSystem.Mouse, Unity.InputSystem")?.GetProperty("current");
        private object keyboard, mouse;
        private Func<bool> forward, back, left, right, up, down, fast, looking;
        private Func<Vector2> position, delta;

        private static Func<bool> Button(object device, string name)
        {
            object control = device?.GetType().GetProperty(name)?.GetValue(device);
            MethodInfo getter = control?.GetType().GetProperty("isPressed")?.GetGetMethod();
            return getter == null ? null : (Func<bool>)Delegate.CreateDelegate(typeof(Func<bool>), control, getter);
        }

        private static Func<Vector2> Vector(object device, string name)
        {
            object control = device?.GetType().GetProperty(name)?.GetValue(device);
            MethodInfo read = control?.GetType().GetMethod("ReadValue", Type.EmptyTypes);
            return read == null ? null : (Func<Vector2>)Delegate.CreateDelegate(typeof(Func<Vector2>), control, read);
        }

        internal bool Read(out bool look, out Vector2 pointer, out Vector2 motion, out Vector3 movement, out bool faster)
        {
            look = faster = false; pointer = motion = Vector2.zero; movement = Vector3.zero;
            object currentKeyboard = keyboardCurrent?.GetValue(null);
            object currentMouse = mouseCurrent?.GetValue(null);
            if (!ReferenceEquals(keyboard, currentKeyboard))
            {
                keyboard = currentKeyboard;
                forward = Button(keyboard, "wKey"); back = Button(keyboard, "sKey");
                left = Button(keyboard, "aKey"); right = Button(keyboard, "dKey");
                up = Button(keyboard, "eKey"); down = Button(keyboard, "qKey");
                fast = Button(keyboard, "leftShiftKey");
            }
            if (!ReferenceEquals(mouse, currentMouse))
            {
                mouse = currentMouse;
                looking = Button(mouse, "rightButton");
                position = Vector(mouse, "position"); delta = Vector(mouse, "delta");
            }
            if (keyboard != null && mouse != null)
            {
                look = looking?.Invoke() ?? false;
                pointer = position?.Invoke() ?? Vector2.zero; motion = delta?.Invoke() ?? Vector2.zero;
                faster = fast?.Invoke() ?? false;
                movement = new Vector3((right?.Invoke() == true ? 1 : 0) - (left?.Invoke() == true ? 1 : 0),
                    (up?.Invoke() == true ? 1 : 0) - (down?.Invoke() == true ? 1 : 0),
                    (forward?.Invoke() == true ? 1 : 0) - (back?.Invoke() == true ? 1 : 0));
                return true;
            }
#if ENABLE_LEGACY_INPUT_MANAGER
            look = Input.GetMouseButton(1); pointer = Input.mousePosition;
            motion = new Vector2(Input.GetAxisRaw("Mouse X"), Input.GetAxisRaw("Mouse Y")) * 10;
            faster = Input.GetKey(KeyCode.LeftShift);
            movement = new Vector3((Input.GetKey(KeyCode.D) ? 1 : 0) - (Input.GetKey(KeyCode.A) ? 1 : 0),
                (Input.GetKey(KeyCode.E) ? 1 : 0) - (Input.GetKey(KeyCode.Q) ? 1 : 0),
                (Input.GetKey(KeyCode.W) ? 1 : 0) - (Input.GetKey(KeyCode.S) ? 1 : 0));
            return true;
#else
            return false;
#endif
        }
    }
}
