// ============================================================================
// MovingEnemy.cs – Waypoint-following enemy with acoustic source
// ============================================================================
using UnityEngine;
using Magnaundasoni;

namespace MagnaundasoniDemo
{
    /// <summary>
    /// Patrols through an array of waypoints continuously. The attached
    /// MagnaundasoniSource updates its position each frame so the acoustics
    /// engine correctly simulates the moving sound.
    /// </summary>
    [RequireComponent(typeof(AudioSource))]
    public class MovingEnemy : MonoBehaviour
    {
        [Header("Patrol")]
        [Tooltip("Waypoint transforms the enemy patrols between.")]
        [SerializeField] private Transform[] _waypoints;

        [Tooltip("Movement speed in meters per second.")]
        [SerializeField] private float _moveSpeed = 3f;

        [Tooltip("Distance threshold to consider a waypoint reached.")]
        [SerializeField] private float _arrivalThreshold = 0.2f;

        [Tooltip("Optional pause time at each waypoint in seconds.")]
        [SerializeField] private float _waitTime = 0.5f;

        private MagnaundasoniSource _acousticSource;
        private int _currentWaypointIndex;
        private float _waitTimer;
        private bool _isWaiting;

        private void Awake()
        {
            _acousticSource = GetComponent<MagnaundasoniSource>();
        }

        private void Update()
        {
            if (_waypoints == null || _waypoints.Length == 0) return;

            if (_isWaiting)
            {
                _waitTimer -= Time.deltaTime;
                if (_waitTimer <= 0f)
                    _isWaiting = false;
                return;
            }

            MoveTowardsWaypoint();
        }

        private void MoveTowardsWaypoint()
        {
            Transform target = _waypoints[_currentWaypointIndex];
            if (target == null) { AdvanceWaypoint(); return; }

            Vector3 direction = target.position - transform.position;
            float distance = direction.magnitude;

            if (distance <= _arrivalThreshold)
            {
                AdvanceWaypoint();
                _isWaiting = _waitTime > 0f;
                _waitTimer = _waitTime;
                return;
            }

            Vector3 moveDir = direction.normalized;
            transform.position += moveDir * _moveSpeed * Time.deltaTime;

            // Face movement direction
            if (moveDir.sqrMagnitude > 0.001f)
                transform.rotation = Quaternion.LookRotation(moveDir);
        }

        private void AdvanceWaypoint()
        {
            _currentWaypointIndex = (_currentWaypointIndex + 1) % _waypoints.Length;
        }

        // ----- Editor Gizmos -----------------------------------------------
#if UNITY_EDITOR
        private void OnDrawGizmosSelected()
        {
            if (_waypoints == null || _waypoints.Length < 2) return;

            Gizmos.color = Color.red;
            for (int i = 0; i < _waypoints.Length; i++)
            {
                if (_waypoints[i] == null) continue;
                Gizmos.DrawSphere(_waypoints[i].position, 0.3f);

                int next = (i + 1) % _waypoints.Length;
                if (_waypoints[next] != null)
                    Gizmos.DrawLine(_waypoints[i].position, _waypoints[next].position);
            }
        }
#endif
    }
}
