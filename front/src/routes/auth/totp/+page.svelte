<script lang="ts">
	import { goto } from '$app/navigation';
	import { resolve } from '$app/paths';
	import { page } from '$app/state';
	import { PUBLIC_API_URL } from '$env/static/public';
	import Button from '$lib/components/Button.svelte';
	import Card from '$lib/components/Card.svelte';
	import FormControl from '$lib/components/Form/FormControl.svelte';
	import Spinner from '$lib/components/Spinner.svelte';

	let status = $state<'form' | 'loading'>('form');
	let code = $state('');
	let errorMessage = $state('');

	const token = $derived(page.url.searchParams.get('token'));

	const handleSubmit = async (
		event: SubmitEvent & { currentTarget: EventTarget & HTMLFormElement }
	) => {
		event.preventDefault();

		if (!token) {
			errorMessage = 'No token provided. Please use the login link you received by email.';
			return;
		}

		status = 'loading';
		errorMessage = '';

		try {
			const response = await fetch(`${PUBLIC_API_URL}/api/auth/login/totp`, {
				method: 'POST',
				body: JSON.stringify({ token, code })
			});

			const result = await response.json();

			if (!response.ok) {
				if (response.status === 429) {
					throw Error('Too many attempts. Please wait a minute before trying again.');
				}

				throw Error(result?.message ?? 'An error occurred');
			}

			window.sessionStorage.setItem('date-now-access-token', result.token);
			window.sessionStorage.setItem('date-now-refresh-token', result.refresh_token);

			goto(resolve('/auth/profile'));
		} catch (e) {
			status = 'form';
			errorMessage = e instanceof Error ? e.message : 'An error occurred';
		}
	};
</script>

<Card centered>
	<h1>TOTP Login</h1>

	{#if !token}
		<p>No token provided. Please use the login link you received by email.</p>

		<Button tag="a" href="/">Go back home</Button>
	{:else if status === 'loading'}
		<Spinner text="Checking your code..." />
	{:else}
		<p>Enter the code from your authenticator app to finish signing in.</p>

		<form class="totp__form" onsubmit={handleSubmit}>
			<FormControl
				id="totp"
				label="Code TOTP"
				required
				bind:value={code}
				inputmode="numeric"
				autocomplete="one-time-code"
				pattern="[0-9]*"
				maxlength={6}
			/>

			{#if errorMessage}
				<p class="totp__error" role="alert">{errorMessage}</p>
			{/if}

			<Button variant="primary" type="submit">Login</Button>
		</form>
	{/if}
</Card>

<style lang="scss">
	.totp {
		&__form {
			display: flex;
			flex-direction: column;
			gap: 12px;
			align-items: end;
			max-width: 320px;
			width: 100%;
		}

		&__error {
			margin: 0;
			align-self: start;
		}
	}
</style>
