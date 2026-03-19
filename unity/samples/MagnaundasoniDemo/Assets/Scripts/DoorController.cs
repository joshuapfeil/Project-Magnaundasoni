// ============================================================================
// DoorController.cs – Animated door with acoustic geometry integration
// ============================================================================
using UnityEngine;
using Magnaundasoni;

namespace MagnaundasoniDemo
{
    /// <summary>
    /// Rotates a door on interact (E key). The attached MagnaundasoniGeometry is
    /// set to DynamicImportant so the engine re-traces geometry each frame while
    /// the door moves, letting audio change as the door opens and closes.
    /// </summary>
    [RequireComponent(typeof(AudioSource))]
    public class DoorController : MonoBehaviour
    {
        [Header("Door Settings")]
        [Tooltip("Maximum rotation angle in degrees when fully open.")]
        [SerializeField] private float _openAngle = 90f;

        [Tooltip("Speed of the door rotation in degrees per second.")]
        [SerializeField] private float _rotationSpeed = 120f;

        [Tooltip("Key to interact with the door.")]
        [SerializeField] private KeyCode _interactKey = KeyCode.E;

        [Tooltip("Maximum distance the player can be to interact.")]
        [SerializeField] private float _interactDistance = 3f;

        [Header("Audio")]
        [SerializeField] private AudioClip _openSound;
        [SerializeField] private AudioClip _closeSound;

        [Header("References")]
        [Tooltip("Optional: the pivot transform for the door. Uses this transform if not set.")]
        [SerializeField] private Transform _pivot;

        private MagnaundasoniGeometry _geometry;
        private AudioSource _audioSource;

        private bool _isOpen;
        private bool _isAnimating;
        private float _currentAngle;
        private float _targetAngle;
        private Quaternion _closedRotation;

        private void Awake()
        {
            _audioSource = GetComponent<AudioSource>();
            _geometry = GetComponent<MagnaundasoniGeometry>();

            if (_pivot == null)
                _pivot = transform;

            _closedRotation = _pivot.localRotation;

            // Ensure geometry is marked as DynamicImportant for real-time updates
            if (_geometry != null)
                _geometry.SetDynamicFlag(MagDynamicFlag.DynamicImportant);
        }

        private void Update()
        {
            if (Input.GetKeyDown(_interactKey) && !_isAnimating && IsPlayerInRange())
                ToggleDoor();

            if (_isAnimating)
                AnimateDoor();
        }

        private void ToggleDoor()
        {
            _isOpen = !_isOpen;
            _targetAngle = _isOpen ? _openAngle : 0f;
            _isAnimating = true;

            AudioClip clip = _isOpen ? _openSound : _closeSound;
            if (clip != null)
                _audioSource.PlayOneShot(clip);
        }

        private void AnimateDoor()
        {
            _currentAngle = Mathf.MoveTowards(_currentAngle, _targetAngle,
                _rotationSpeed * Time.deltaTime);

            _pivot.localRotation = _closedRotation * Quaternion.Euler(0f, _currentAngle, 0f);

            if (Mathf.Approximately(_currentAngle, _targetAngle))
                _isAnimating = false;
        }

        private bool IsPlayerInRange()
        {
            Camera cam = Camera.main;
            if (cam == null) return false;
            return Vector3.Distance(cam.transform.position, transform.position) <= _interactDistance;
        }
    }
}
