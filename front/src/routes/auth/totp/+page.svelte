<script lang="ts">
	import { goto } from '$app/navigation';
	import Card from '$lib/components/Card.svelte';

	async function handleSubmit(
		event: SubmitEvent & { currentTarget: EventTarget & HTMLFormElement }
	) {
		event.preventDefault();

		try {
			const urlParams = new URLSearchParams(window.location.search);
			const token = urlParams.get('token');

			if (!token) {
				throw Error('No token provided');
			}

			const data = {
				token: token,
				code: event.target?.querySelector('#totp').value
			};

			const response = await fetch('http://localhost:8000/api/auth/login/totp', {
				method: 'POST',
				body: JSON.stringify(data)
			});

			const result = await response.json();

			if (result) {
				if (result.code >= 400) {
					throw Error(result.message);
				}

				window.sessionStorage.setItem('date-now-access-token', result.token);
				window.sessionStorage.setItem('date-now-refresh-token', result.refresh_token);

				goto('/auth/profile');
			}
		} catch (e) {
			console.error(e);
		}
	}
</script>

<Card>
	TOTP Login

	<form onsubmit={handleSubmit}>
		<label for="totp">Code TOTP</label>
		<input id="totp" required />

		<button type="submit">Login</button>
	</form>
</Card>
